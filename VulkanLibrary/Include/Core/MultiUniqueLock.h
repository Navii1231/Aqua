#pragma once
#include "Config.h"

VK_BEGIN

template <typename Mtx>
class UniqueLockRange {
public:
	UniqueLockRange() = default;

	UniqueLockRange(const std::span<Mtx* const>& mtxs)
	{
		Init(mtxs.begin(), mtxs.end());
		LockAll();
	}

	~UniqueLockRange() 
	{
		UnlockAll();
	}

	UniqueLockRange(const UniqueLockRange&) = delete;
	UniqueLockRange& operator=(const UniqueLockRange&) = delete;

	UniqueLockRange(UniqueLockRange&& other) noexcept
		: mMutexes(std::move(other.mMutexes)), mLocked(other.mLocked) 
	{ other.mLocked = false; }

	UniqueLockRange& operator=(UniqueLockRange&& other) noexcept 
	{
		if (this != &other) 
		{
			UnlockAll();
			mMutexes = std::move(other.mMutexes);
			mLocked = other.mLocked;
			other.mLocked = false;
		}

		return *this;
	}

	template <class It>
	void reset(It first, It last)
	{
		UnlockAll();
		Init(first, last);
		LockAll();
	}

	void Lock() { if (!mLocked) LockAll(); }
	void Unlock() { UnlockAll(); }
	bool OwnsLock() const noexcept { return mLocked; }

private:
	std::vector<Mtx*> mMutexes;
	bool mLocked = false;

private:
	template <class It>
	void Init(It first, It last)
	{
		mMutexes.assign(first, last);
		// drop nulls
		mMutexes.erase(std::remove(mMutexes.begin(), mMutexes.end(), nullptr), mMutexes.end());
		// impose a total order & dedupe so all threads lock in the same order
		std::sort(mMutexes.begin(), mMutexes.end());
		mMutexes.erase(std::unique(mMutexes.begin(), mMutexes.end()), mMutexes.end());
	}

	void LockAll() 
	{
		for (auto* m : mMutexes) m->lock();
		mLocked = true;
	}

	void UnlockAll() noexcept 
	{
		if (!mLocked) return;

		for (auto it = mMutexes.rbegin(); it != mMutexes.rend(); ++it) 
		{
			(*it)->unlock();
		}

		mLocked = false;
	}
};

VK_END
