#pragma once
#include "WorkerQueue.h"
#include "WorkingClass.h"

#include "../Core/MultiUniqueLock.h"

#include "../Core/Ref.h"

VK_BEGIN


VKLIB_API vk::Result WaitForWorkers(vk::ArrayProxy<Core::Worker> workers, bool waitAll = true, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

VK_CORE_BEGIN

// we could allow the user to insert std::semaphores or std::condition_variables into the submit functions for CPU synchronization
// Once the queue finishes the task, these sync elements are removed from the queues and it's now up to the user to check their values
class Worker
{
public:
	Worker() = default;

	VKLIB_API uint32_t Dispatch(vk::ArrayProxy<vk::SubmitInfo> submitInfos,
		std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;

	VKLIB_API uint32_t Dispatch(vk::ArrayProxy<vk::CommandBuffer> cmdBuffer,
		std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;

	VKLIB_API uint32_t Dispatch(vk::ArrayProxy<vk::Semaphore> signalSemaphores,
		vk::ArrayProxy<vk::CommandBuffer> buffers,
		std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;

	VKLIB_API uint32_t Dispatch(vk::ArrayProxy<QueueWaitingPoint> waitPoint,
		vk::ArrayProxy<vk::Semaphore> signalSemaphores, 
		vk::ArrayProxy<vk::CommandBuffer> buffers,
		std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;

	Ref<WorkerQueue> NextQueue() const { return mFamilyData->Workers[mFamilyData->Next()]; }

	VKLIB_API vk::Result WaitIdle(std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;
	VKLIB_API bool IsFree() const;

	vk::Device GetDeviceHandle() const { return *mDevice; }
	vk::Fence GetFence() const { return *mFence; }
	uint32_t GetFamilyIndex() const { return mFamilyData->Index; }
	size_t GetCount() const { return mFamilyData->Workers.size(); }

	auto begin() const { return mFamilyData->Workers.begin(); }
	auto end() const { return mFamilyData->Workers.end(); }

	Ref<WorkerQueue> operator[](size_t index) const { return mFamilyData->Workers[index]; }

	bool operator ==(const Worker& Other) const = default;
	bool operator !=(const Worker& Other) const { return !(*this == Other); }

private:
	const WorkerFamily* mFamilyData = nullptr;

	vkLib::Core::Ref<vk::Fence> mFence;
	vkLib::Core::Ref<vk::Device> mDevice;

	friend class ::VK_NAMESPACE::WorkingClass;
	friend vk::Result VK_NAMESPACE::WaitForWorkers(vk::ArrayProxy<Worker> workers, 
		bool waitAll, std::chrono::nanoseconds timeout);

private:
	Worker(vkLib::Core::Ref<vk::Device> device, const WorkerFamily* familyData)
		: mDevice(device), mFamilyData(familyData)
	{
		mFence = vkLib::Core::CreateRef<vk::Fence>(
		[device](vk::Fence& fence)
		{
			// wait for all the processes to finish before destroying the fence
			auto result = device->waitForFences(fence, VK_TRUE, UINT64_MAX);

			_STL_VERIFY(result == vk::Result::eSuccess, "Worker fence couldn't be waited on");

			device->destroyFence(fence);
		}, device->createFence({ vk::FenceCreateFlagBits::eSignaled }));
	}

	size_t GetBeginIdx() const
	{ return 0; }

	size_t GetEndIdx() const
	{ return mFamilyData->Workers.size(); }

	std::span<const Ref<WorkerQueue>> GetActiveWorkers() const;
	vk::Result WaitForQueues(size_t beginSpan, size_t endSpan, bool waitForAll, std::chrono::nanoseconds timeOut) const;

	uint32_t SubmitRange(const vk::ArrayProxy<vk::SubmitInfo>& submitInfos, std::chrono::nanoseconds timeOut) const;

	template <typename Fn>
	uint32_t PollFreeWorkers(const std::chrono::nanoseconds timeOut, Fn&& fn) const;
};

template <typename Fn>
uint32_t VK_NAMESPACE::VK_CORE::Worker::PollFreeWorkers(std::chrono::nanoseconds timeOut, Fn&& fn) const
{
	// make sure no external process can submit to any worker while this routine isn't complete
	// a centralized mutex is slightly worse for the performance...
	auto workers = GetActiveWorkers();

	std::span mtxSpan(mFamilyData->mMutexes.begin() + GetBeginIdx(),
		mFamilyData->mMutexes.begin() + GetEndIdx());

	UniqueLockRange lockRange(mtxSpan);

	if (WaitForQueues(GetBeginIdx(), GetEndIdx(), false, timeOut) != vk::Result::eSuccess)
		return std::numeric_limits<uint32_t>::max();

	// polling the workers to submit the work
	for (auto worker : workers)
	{
		// checkout which fence/worker was freed
		if (worker->WaitIdleAsync(0) != vk::Result::eSuccess)
			continue;

		if (fn(worker))
			return static_cast<uint32_t>(worker->GetWorkerID());
	}

	// if everything goes right, we should never hit this line of code...
	return std::numeric_limits<uint32_t>::max();
}

VK_CORE_END
VK_END
