#pragma once
#include "../Core/AqCore.h"
#include "../Core/SharedRef.h"

// the library is compatible with the c++17

AQUA_BEGIN

#define OLDER_VERSION  0

enum class ThreadPoolStatus
{
	eUnderloaded     = 0,
	eOverloaded      = 1,
};

using ThreadFn = std::function<void()>;
using ThreadInfos = std::map<std::thread::id, std::mutex*>;

template <typename RetType>
using PackagedTask = std::packaged_task<RetType()>;

template <typename RetType>
using Future = std::shared_future<RetType>;

template <typename Fn, typename ...ARGS>
SharedRef<PackagedTask<typename std::invoke_result<Fn, ARGS...>::type>> BindTask(Fn&& fn, ARGS&& ...args)
{
	using RetType = typename std::invoke_result<Fn, ARGS...>::type;

	// we'll use tuples for storing the arguments
	std::tuple<typename std::decay<ARGS>::type...> futureArgs(std::forward<ARGS>(args)...);

	typename std::decay<Fn>::type myFn = std::forward<Fn>(fn);

	return MakeRef<PackagedTask<RetType>>([myFn, futureArgs]()->RetType
	{
		return std::apply(myFn, futureArgs);
	});
}

/// characteristics a scheduler should have
// it should accept arbitrary tasks and store them properly
// one should be able to access those tasks according to certain strategy
// so it should be something like
/*
* Scheduler scheduler;
* scheduler.StoreTaskQueue(...);
* scheduler.StoreTask(...);
* we could use a multiple type of algorithms to access the "next" task
* auto task = scheduler.GetNextTask();
* whether a task can be interrupted in the middle by a user program is still debatable
*/

template <typename Fn, typename ...ARGS>
struct TaskBinder
{
	using RetType = typename std::invoke_result<Fn, ARGS...>::type;
	SharedRef<PackagedTask<RetType>> mTask;

	TaskBinder(Fn&& fn, ARGS&&... args)
	{
		mTask = BindTask(std::forward<Fn>(fn), std::forward<ARGS>(args)...);
	}

	ThreadFn GetThreadFn() const
	{
		auto task = mTask;
		return [task]() { task->operator()(); };
	}

	Future<RetType> GetFuture() const { return mTask->get_future().share(); }
};

inline void FillLocks(std::forward_list<std::unique_lock<std::mutex>>& locks, const ThreadInfos& threadInfos)
{
	for (const auto& [ID, lock] : threadInfos)
	{
		locks.emplace_front(*lock);
	}
}

template <bool _Bounded>
class DefaultPolicy
{
public:
	struct PolicyInfo
	{
		std::queue<ThreadFn> mTasks{};
		std::atomic_uint64_t mTaskCount{};
		ThreadInfos mThreadInfos{};
	};

public:
	~DefaultPolicy() = default;
	DefaultPolicy() : mInfo(MakeRef<PolicyInfo>()) {}

	// the thread pool will make sure that this function is thread safe
	void operator()(const ThreadInfos& ids)
	{
		mInfo->mThreadInfos = ids;
	}

	bool operator()(ThreadFn fn)
	{
		if constexpr (_Bounded)
		{
			if (mInfo->mTaskCount.load() >= mInfo->mThreadInfos.size())
				return false;
		}

		mInfo->mTasks.push(fn);
		mInfo->mTaskCount++;

		return true;
	}

	ThreadFn operator()()
	{
		std::forward_list<std::unique_lock<std::mutex>> locks{};
		FillLocks(locks, mInfo->mThreadInfos);

		if (mInfo->mTasks.empty())
			return {};

		auto fn = mInfo->mTasks.front();

		mInfo->mTasks.pop();
		mInfo->mTaskCount--;

		return fn;
	}

protected:
	SharedRef<PolicyInfo> mInfo{};
};

using BoundedDefaultPolicy = DefaultPolicy<true>;
using UnboundedDefaultPolicy = DefaultPolicy<false>;

struct ThreadPoolInfo
{
	std::function<ThreadFn()> mScheduler{};
	std::function<bool(ThreadFn)> mTaskManager{};
	std::function<void(const ThreadInfos&)> mThreadInfoFunctor{};

	std::atomic_uint64_t mTaskCount;

	std::condition_variable mWorkerNotifier;
	ThreadInfos mThreadInfos;
};

struct ThreadWorker
{
	SharedRef<ThreadPoolInfo> mPoolInfo;

	std::thread mHandle;
	std::atomic_bool mAlive = true;
	mutable std::mutex mLock;
	std::atomic<std::chrono::nanoseconds> mWaitTime = std::chrono::nanoseconds::max();

	ThreadWorker(SharedRef<ThreadPoolInfo> poolInfo)
		: mPoolInfo(poolInfo), mHandle(&ThreadWorker::Dispatch, this) {}

	~ThreadWorker()
	{
		{
			// the worker is about to be killed...
			// so get the most out of it before it's gone
			std::scoped_lock locker(mLock);

			mAlive.store(false);
			mPoolInfo->mWorkerNotifier.notify_all();
		}

		mHandle.join();
	}

	ThreadWorker(const ThreadWorker&) = delete;
	ThreadWorker& operator=(const ThreadWorker&) = delete;

	template <typename _Dur>
	void SetWaitingTime(_Dur dur) { mWaitTime.store(dur); }

	void Dispatch() const
	{
		// keep looping until the tasks remain or the worker is alive
		while (mAlive.load() || mPoolInfo->mTaskCount.load())
		{
			// access the lock
			{
				std::unique_lock locker(mLock);
				mPoolInfo->mWorkerNotifier.wait_for(locker, mWaitTime.load(), [this]()
					{
						// either we've a remaining task or the worker is no longer alive
						return mPoolInfo->mTaskCount.load() || !mAlive.load();
					});
			}

			auto fn = mPoolInfo->mScheduler();
			mPoolInfo->mTaskCount.fetch_sub(bool(fn));

			if (fn)
				fn();
		}
	}
};

class ThreadPool
{
public:
	inline ThreadPool();
	inline explicit ThreadPool(uint32_t threadCount);

	~ThreadPool() { Clear(); }

	inline std::thread::id Create();
	inline void Free(uint32_t idx);

	template <typename _Scheduler>
	void SetScheduler(_Scheduler scheduler)
	{
		mInfo->mThreadInfoFunctor = scheduler;
		mInfo->mScheduler = scheduler;
		mInfo->mTaskManager = scheduler;

		std::forward_list<std::unique_lock<std::mutex>> locks;
		FillLocks(locks, mInfo->mThreadInfos);

		scheduler(mInfo->mThreadInfos);
	}

	template <typename Fn, typename ...ARGS>
	auto Enqueue(Fn&& fn, ARGS&&... args) -> Future<typename TaskBinder<Fn, ARGS...>::RetType>
	{
		TaskBinder<Fn, ARGS...> taskBinder(std::forward<Fn>(fn), std::forward<ARGS>(args)...);

		{
			std::forward_list<std::unique_lock<std::mutex>> locks{};
			FillLocks(locks, mInfo->mThreadInfos);

			if (!InsertTask(taskBinder.GetThreadFn()))
				return {};
		}

		return taskBinder.GetFuture();
	}

	void Clear()
	{
		size_t size = mWorkers.size();

		for (; size != 0; size--)
		{
			Free(0);
		}
	}

	uint32_t GetWorkerCount() const { return static_cast<uint32_t>(mWorkers.size()); }

	const ThreadInfos& GetThreadIDs() const { return mInfo->mThreadInfos; }

private:
	SharedRef<ThreadPoolInfo> mInfo;
	std::map<std::thread::id, SharedRef<ThreadWorker>> mWorkers;

private:
	inline bool InsertTask(const ThreadFn& task);

	inline std::thread::id CreateImpl();
	inline void FreeImpl(std::thread::id idx);

	void SetDefaultExecutionPolicy()
	{
		DefaultPolicy<false> policy{};
		SetScheduler(policy);
	}
};

ThreadPool::ThreadPool()
{
	mInfo = MakeRef<ThreadPoolInfo>();
	SetDefaultExecutionPolicy();
}

ThreadPool::ThreadPool(uint32_t threadCount)
{
	mInfo = MakeRef<ThreadPoolInfo>();
	
	for (uint32_t i = 0; i < threadCount; i++)
	{
		CreateImpl();
	}

	SetDefaultExecutionPolicy();
}

void AQUA_NAMESPACE::ThreadPool::Free(uint32_t idx)
{
	auto it = mWorkers.begin();

	for (; idx != 0; idx--)
	{
		it++;
	}

	FreeImpl(it->second->mHandle.get_id());
}

std::thread::id AQUA_NAMESPACE::ThreadPool::Create()
{
	auto threadID = CreateImpl();

	std::forward_list<std::unique_lock<std::mutex>> locks;
	FillLocks(locks, mInfo->mThreadInfos);

	mInfo->mThreadInfoFunctor(mInfo->mThreadInfos);

	return threadID;
}

bool ThreadPool::InsertTask(const ThreadFn& task)
{
	if (!mInfo->mTaskManager(task))
		return false;

	mInfo->mWorkerNotifier.notify_one();
	mInfo->mTaskCount++;

	return true;
}

std::thread::id ThreadPool::CreateImpl()
{
	auto worker = MakeRef<ThreadWorker>(mInfo);

	mWorkers[worker->mHandle.get_id()] = worker;
	mInfo->mThreadInfos[worker->mHandle.get_id()] = &worker->mLock;

	return worker->mHandle.get_id();
}

void ThreadPool::FreeImpl(std::thread::id idx)
{
	mWorkers.erase(idx);
	mInfo->mThreadInfos.erase(idx);

	std::forward_list<std::unique_lock<std::mutex>> locks;
	FillLocks(locks, mInfo->mThreadInfos);

	mInfo->mThreadInfoFunctor(mInfo->mThreadInfos);
}

AQUA_END
