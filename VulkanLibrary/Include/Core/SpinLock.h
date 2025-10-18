#pragma once
#include "Config.h"

VK_BEGIN

class SpinLock
{
public:
	SpinLock()
		: mSignal(false) {}

	~SpinLock() = default;

	SpinLock(const SpinLock&) = delete;
	SpinLock& operator=(const SpinLock&) = delete;

	void lock()
	{
		while (true)
		{
			if (!try_lock()) return;

			for (volatile int i = 0; i < 4; i++)
				_mm_pause();

			while (!mSignal.load());
		}

		std::mutex mtx;
	}

	bool try_lock() { return mSignal.exchange(true); }

	void unlock() { mSignal.store(false); }

private:
	std::atomic_bool mSignal;
};

VK_END
