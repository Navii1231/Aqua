#pragma once
#include "../Core/AqCore.h"
#include "../Core/SharedRef.h"

// the library is compatible with the c++17

AQUA_BEGIN

enum class ThreadPoolStatus
{
	eUnderloaded     = 0,
	eOverloaded      = 1,
};

using ThreadFn = std::function<void()>;

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

struct ThreadPoolInfo
{
	std::queue<ThreadFn> mTasks;
	std::atomic_uint64_t mTaskCount;

	std::atomic_uint64_t mBusyThreadCount;

	std::mutex mLock;
	std::condition_variable mWorkerNotifier;
	std::condition_variable mEmptyNotifier;
};

struct ThreadExecutor
{
	SharedRef<ThreadPoolInfo> mPoolInfo;

	std::thread mHandle;
	std::atomic_bool mAlive = true;

	mutable std::atomic_uint64_t mTaskCount = 0;
	mutable std::queue<ThreadFn> mTasks;

	mutable std::mutex mEmptyLock{};
	mutable std::condition_variable mEmptyNotifier{};

	ThreadExecutor(SharedRef<ThreadPoolInfo> poolInfo)
		: mPoolInfo(poolInfo), mHandle(&ThreadExecutor::Dispatch, this) {}

	~ThreadExecutor() { mHandle.join(); }

	ThreadExecutor(const ThreadExecutor&) = delete;
	ThreadExecutor& operator=(const ThreadExecutor&) = delete;

	inline void Dispatch() const;

	inline ThreadFn GrabTask() const;
	inline ThreadFn GrabTaskFromPool(std::queue<ThreadFn>& tasks, std::atomic_uint64_t& taskCount) const;

	inline void NotifyOnBeingEmpty() const
	{
		std::scoped_lock locker(mPoolInfo->mLock);

		if (mTaskCount.load() == 0)
			mEmptyNotifier.notify_all();

		if (mPoolInfo->mTaskCount.load() == 0)
			mPoolInfo->mEmptyNotifier.notify_all();
	}
};

// it's allocated by the thread pool but can later detach and exist independently
class ThreadWorker
{
public:
	ThreadWorker() = default;
	inline ~ThreadWorker();

	template <typename Fn, typename ...ARGS>
	auto Enqueue(Fn&& fn, ARGS&&... args) -> Future<typename TaskBinder<Fn, ARGS...>::RetType>
	{
		TaskBinder<Fn, ARGS...> taskBinder(std::forward<Fn>(fn), std::forward<ARGS>(args)...);

		{
			std::scoped_lock locker(mPoolInfo->mLock);
			InsertTask(taskBinder.GetThreadFn());
		}

		return taskBinder.GetFuture();
	}

	inline void Wait(std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) const
	{
		std::unique_lock locker(mInfo->mEmptyLock);
		mInfo->mEmptyNotifier.wait_for(locker, timeout, [this]()
			{
				return mInfo->mTasks.empty();
			});
	}

	bool IsEmpty() const { return mInfo->mTaskCount.load() == 0; }

	std::thread::id GetThreadID() const { return mInfo->mHandle.get_id(); }

private:
	SharedRef<ThreadExecutor> mInfo;
	SharedRef<ThreadPoolInfo> mPoolInfo;

	// the worker thread can consist of its own tasks
	// those tasks are always prioritized

private:
	ThreadWorker(SharedRef<ThreadPoolInfo> poolInfo)
		: mPoolInfo(poolInfo)
	{
		mInfo = MakeRef<ThreadExecutor>(poolInfo);
	}

	inline void InsertTask(const ThreadFn& fn);

	friend class ThreadPool;
};

class ThreadPool
{
public:
	inline ThreadPool();
	inline explicit ThreadPool(uint32_t threadCount);

	~ThreadPool() {}

	inline ThreadWorker Create();

	template <typename Fn, typename ...ARGS>
	ThreadWorker Create(Fn&& fn, ARGS&&... args)
	{
		auto worker = Create();
		worker.Enqueue(std::forward<Fn>(fn), std::forward<ARGS>(args)...);
		return worker;
	}

	inline void Free(uint32_t idx);

	template <typename Fn, typename ...ARGS>
	auto Enqueue(Fn&& fn, ARGS&&... args) -> Future<typename TaskBinder<Fn, ARGS...>::RetType>
	{
		TaskBinder<Fn, ARGS...> taskBinder(std::forward<Fn>(fn), std::forward<ARGS>(args)...);

		{
			std::scoped_lock locker(mInfo->mLock);
			InsertTask(taskBinder.GetThreadFn());
		}

		return taskBinder.GetFuture();
	}

	template <typename Fn, typename ...ARGS>
	auto EnqueueOnlyIfFree(Fn&& fn, ARGS&&... args) -> std::expected<Future<typename TaskBinder<Fn, ARGS...>::RetType>, ThreadPoolStatus>
	{
		std::scoped_lock locker(mInfo->mLock);

		if (IsOverloaded())
			return std::unexpected(ThreadPoolStatus::eOverloaded);

		TaskBinder<Fn, ARGS...> taskBinder(std::forward<Fn>(fn), std::forward<ARGS>(args)...);
		InsertTask(taskBinder.GetThreadFn());
		return taskBinder.GetFuture();
	}

	ThreadWorker operator[](uint32_t idx) { return mWorkers[idx]; }
	void Clear() { mWorkers.clear(); }

	bool IsOverloaded() const { return mInfo->mBusyThreadCount.load() >= mWorkers.size(); }

	inline void Wait(std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) const;

	uint32_t GetWorkerCount() const { return static_cast<uint32_t>(mWorkers.size()); }

private:
	SharedRef<ThreadPoolInfo> mInfo;
	std::vector<ThreadWorker> mWorkers;

private:
	inline void InsertTask(const std::function<void()>& task);
};

AQUA_END

AQUA_NAMESPACE::ThreadWorker::~ThreadWorker()
{
	if (mInfo.use_count() != 1)
		return;

	// the worker is about to be killed...
	// so get the most out of it before it's gone

	std::scoped_lock locker(mPoolInfo->mLock);

	mInfo->mAlive.store(false);
	mPoolInfo->mWorkerNotifier.notify_all();
}

void AQUA_NAMESPACE::ThreadExecutor::Dispatch() const
{
	// keep looping until the tasks remain or the worker is alive
	while (mAlive.load() || mPoolInfo->mTaskCount.load() || mTaskCount.load())
	{
		ThreadFn threadFn{};

		{
			// access the lock
			std::unique_lock locker(mPoolInfo->mLock);
			mPoolInfo->mWorkerNotifier.wait_for(locker, std::chrono::nanoseconds::max(), [this]()
			{
				// either we've a remaining task or the worker is no longer alive
				return mPoolInfo->mTaskCount.load() || mTaskCount.load() != 0 || !mAlive.load();
			});

			// try grabbing the task
			threadFn = GrabTask();
		}

		// if the task is available, execute it
		if (threadFn)
		{
			threadFn();
			// unbusy thread
			mPoolInfo->mBusyThreadCount.fetch_sub(1);
		}
		
		NotifyOnBeingEmpty();
	}
}

AQUA_NAMESPACE::ThreadFn AQUA_NAMESPACE::ThreadExecutor::GrabTask() const
{
	auto task = GrabTaskFromPool(mTasks, mTaskCount);

	if (task)
		return task;

	return GrabTaskFromPool(mPoolInfo->mTasks, mPoolInfo->mTaskCount);
}

AQUA_NAMESPACE::ThreadFn AQUA_NAMESPACE::ThreadExecutor::GrabTaskFromPool(
	std::queue<ThreadFn>& tasks, std::atomic_uint64_t& taskCount) const
{
	if (tasks.empty())
		return {};

	auto task = tasks.front();
	tasks.pop();

	taskCount--;

	return task;
}

void AQUA_NAMESPACE::ThreadWorker::InsertTask(const ThreadFn& fn)
{
	mInfo->mTasks.push(fn);
	mInfo->mTaskCount++;
	mPoolInfo->mBusyThreadCount++; // busy thread
	mPoolInfo->mWorkerNotifier.notify_all();
}

AQUA_NAMESPACE::ThreadPool::ThreadPool()
{
	mInfo = MakeRef<ThreadPoolInfo>();
}

AQUA_NAMESPACE::ThreadPool::ThreadPool(uint32_t threadCount)
{
	mInfo = MakeRef<ThreadPoolInfo>();

	mWorkers.reserve(threadCount);

	for (uint32_t i = 0; i < threadCount; i++)
	{
		Create();
	}
}

AQUA_NAMESPACE::ThreadWorker AQUA_NAMESPACE::ThreadPool::Create()
{
	mWorkers.push_back(ThreadWorker(mInfo));
	return mWorkers.back();
}

void AQUA_NAMESPACE::ThreadPool::Free(uint32_t idx)
{
	mWorkers.erase(mWorkers.begin() + idx);
}

void AQUA_NAMESPACE::ThreadPool::Wait(std::chrono::nanoseconds timeout) const
{
	std::unique_lock locker(mInfo->mLock);

	// TODO: latency problem :)
	mInfo->mEmptyNotifier.wait_for(locker, timeout, [this]()
		{
			bool AllEmpty = mInfo->mTasks.empty();

			for (const auto& worker : mWorkers)
				AllEmpty &= worker.IsEmpty();

			return AllEmpty;
		});
}

void AQUA_NAMESPACE::ThreadPool::InsertTask(const std::function<void()>& task)
{
	mInfo->mTasks.push(task);
	mInfo->mTaskCount++;
	mInfo->mBusyThreadCount++; // busy thread
	mInfo->mWorkerNotifier.notify_one();
}

