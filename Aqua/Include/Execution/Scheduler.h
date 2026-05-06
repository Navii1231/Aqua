#pragma once
#include "GraphConfig.h"
#include "../Utils/ThreadPool.h"

AQUA_BEGIN
EXEC_BEGIN

// thread safe ofc
// can be shared across multiple thread pools
// TODO: hasn't been tested yet
template <bool _Bounded>
class WorkStealingScheduler
{
public: 
	using TaskBucket = std::tuple<std::deque<ThreadFn>, std::mutex*>;
	using TaskBucketRef = SharedRef<TaskBucket>;
	using BucketMap = std::map<std::thread::id, TaskBucketRef>;

	struct PolicyInfo
	{
		BucketMap mTasks{};
		ThreadInfos mThreadInfos{};
		std::atomic_uint64_t mTaskCount{};
	};

public:
	~WorkStealingScheduler() = default;
	WorkStealingScheduler() : mInfo(MakeRef<PolicyInfo>()) {}

	void operator()(const ThreadInfos& threadIDs)
	{
		mInfo->mThreadInfos = threadIDs;

		std::forward_list<std::unique_lock<std::mutex>> locks{};
		FillLocks(locks);

		ReserveThreads(threadIDs);
	}

	// emplacing the task
	// might say that this function might only practically be 
	// usable inside the ThreadPool
	// these two functions could be separated
	bool operator()(ThreadFn fn)
	{
		if constexpr (_Bounded)
		{
			if (mInfo->mTaskCount.load() >= mInfo->mThreadInfos.size())
				return false;
		}

		std::thread::id ID{};

		{
			std::forward_list<std::unique_lock<std::mutex>> locks{};
			FillLocks(locks);

			ID = GetLeastContentiousThreadID();
		}

		// maybe I could use a spin lock
		auto& [buc, lock] = *mInfo->mTasks[ID];

		std::unique_lock locker(*lock);

		mInfo->mTasks[ID]->push_front(fn);
		mInfo->mTaskCount++;

		return true;
	}

	// running the task
	ThreadFn operator()()
	{
		_STL_ASSERT(false, "work stealing scheduler hasn't been tested yet, use it at your own risk lol");

		auto thisThreadID = std::this_thread::get_id();

		// one access, very unlikely that a two threads might collide here
		// may use spin locks if performance is an issue

		auto& [buc, lock] = *mInfo->mTasks[thisThreadID];

		std::unique_lock locker(*lock);
		ThreadFn fn = GetThreadFn(thisThreadID);

		if (fn)
			return fn;

		// stealing tasks
		std::thread::id mostContentiousID{};

		{
			std::forward_list<std::unique_lock<std::mutex>> locks{};
			FillLocks(locks);

			mostContentiousID = GetMostContentiousThreadID();
		}

		if (mostContentiousID == thisThreadID)
			return {};

		auto& [contBuc, contLock] = *mInfo->mTasks[mostContentiousID];

		std::unique_lock contentiousLocker(*contLock);
		return GetThreadFn(mostContentiousID);
	}

	int64_t GetTaskCount() const { return mInfo->mTaskCount.load(); }

protected:
	SharedRef<PolicyInfo> mInfo{};

protected:
	void FillLocks(std::forward_list<std::unique_lock<std::mutex>>& locks) const
	{
		for (const auto& [ID, tup] : mInfo->mTasks)
		{
			const auto& [buc, lock] = tup;
			locks.emplace_front(*lock);
		}
	}

	void ReserveThreads(const std::map<std::thread::id, std::mutex*>& threadIDs)
	{
		for (auto [ID, lock] : threadIDs)
		{
			auto it = mInfo->mTasks.find(ID);

			if (it != mInfo->mTasks.end())
				continue;

			mInfo->mTasks[ID] = MakeRef<TaskBucket>();
			*mInfo->mTasks[ID] = std::make_tuple(ID, lock);
		}
	}

	std::thread::id GetLeastContentiousThreadID() const
	{
		uint64_t minCount = std::numeric_limits<uint64_t>::max();
		std::thread::id minID = std::thread::id();

		for (const auto& [ID, buc] : mInfo->mTasks)
		{
			if (minCount > buc->mTasks.size())
			{
				minCount = buc->mTasks.size();
				minID = ID;
			}
		}

		return minID;
	}

	std::thread::id GetMostContentiousThreadID() const
	{
		uint64_t maxCount = std::numeric_limits<uint64_t>::min();
		std::thread::id maxID = std::thread::id();

		for (const auto& [ID, buc] : mInfo->mTasks)
		{
			if (maxCount < buc->mTasks.size())
			{
				maxCount = buc->mTasks.size();
				maxID = ID;
			}
		}

		return maxID;
	}

	ThreadFn GetThreadFn(std::thread::id id)
	{
		auto& [currBuc, lock] = *GetBucket(id);

		if (!currBuc.empty())
			return {};

		auto fn = currBuc.back();
		currBuc.pop_back();
		mInfo->mTaskCount--;

		return fn;
	}

	TaskBucketRef GetBucket(std::thread::id id)
	{
		return mInfo->mTasks[id];
	}
};

using BoundedWorkStealingScheduler = WorkStealingScheduler<false>;
using UnboundedWorkStealingScheduler = WorkStealingScheduler<false>;

EXEC_END
AQUA_END
