#pragma once
#include "ProcessConfig.h"
#include "../Core/Ref.h"
#include "../Core/SpinLock.h"

VK_BEGIN

class Context;

VK_CORE_BEGIN

class Worker;

// Thin wrapper over vk::Queue and it's corresponding vk::Fence
// Thread safe
class WorkerQueue
{
public:
	WorkerQueue() = default;

	// A worker is an atomic unit, it's lifetime lasts as long as the context lasts
	WorkerQueue(const WorkerQueue&) = delete;
	WorkerQueue& operator=(const WorkerQueue&) = delete;

	VKLIB_API vk::Result Submit(vk::ArrayProxy<vk::CommandBuffer> buffers, vk::Fence fence = nullptr, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;
	VKLIB_API vk::Result Submit(vk::ArrayProxy<vk::Semaphore> signalSemaphores, vk::ArrayProxy<vk::CommandBuffer> buffers, vk::Fence fence = nullptr, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());
	VKLIB_API vk::Result Submit(vk::ArrayProxy<QueueWaitingPoint> waitPoint, vk::ArrayProxy<vk::Semaphore> signalSemaphores, vk::ArrayProxy<vk::CommandBuffer> buffers, vk::Fence fence = nullptr, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());
	VKLIB_API vk::Result Submit(vk::ArrayProxy<vk::SubmitInfo> submitInfos, vk::Fence fence = nullptr, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;

	VKLIB_API vk::Result BindSparse(const vk::BindSparseInfo& bindSparseInfo, vk::Fence fence = nullptr, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max()) const;

	VKLIB_API vk::Result PresentKHR(const vk::PresentInfoKHR& presentInfo) const;
	VKLIB_API vk::Result WaitIdle() const;

	std::thread::id GetWorkerThreadID() const { return mThreadID; }
	WorkerFamily* GetQueueFamilyInfo() const { return mFamilyInfo; }
	uint32_t GetWorkerID() const { return mWorkerID; }

private:
	vk::Queue mHandle{};

	uint32_t mWorkerID = -1;
	WorkerFamily* mFamilyInfo  = nullptr;

	// keeps track of the thread id on which the WorkerQueue is working
	mutable std::thread::id mThreadID;

	// sync stuff
	mutable WorkerLock mLock;

	vk::Device mDevice;

	WorkerQueue(vk::Queue handle, uint32_t queueIndex, vk::Device device)
		: mHandle(handle), mWorkerID(queueIndex), mDevice(device) {}

	void SubmitInternal(vk::ArrayProxy<vk::SubmitInfo> submitInfos, vk::Fence fence = nullptr) const;
	void BindSparseInternal(const vk::BindSparseInfo& bindSparseInfo, vk::Fence fence = nullptr) const;

	// here the binary_semaphore will be signaled once the process is over
	vk::Result WaitIdleAsync(uint64_t timeout, vk::Fence fence = nullptr) const;

	void SaveThreadID() const;

	friend class Context;
	friend class WorkingClass;
	friend class Worker;

	template <typename T, typename Deleter>
	friend class ControlBlock;
};

VK_CORE_END
VK_END
