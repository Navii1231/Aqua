#include "Core/vkpch.h"
#include "Process/WorkerQueue.h"

vk::Result VK_NAMESPACE::VK_CORE::WorkerQueue::Submit(vk::ArrayProxy<vk::CommandBuffer> buffers,
	 vk::Fence fence, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(buffers);

	return Submit(submitInfo, fence, timeOut);
}

vk::Result VK_NAMESPACE::VK_CORE::WorkerQueue::Submit(vk::ArrayProxy<vk::SubmitInfo> submitInfos,
	 vk::Fence fence, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	// making sure no two threads are accessing this function simultaneously
	std::unique_lock locker(mLock);

	// no two threads will access this sacred space at once!!
	// todo: idk if we need it
	auto result = WaitIdleAsync(timeOut.count(), fence);

	// making sure the previous work is finished before continuing
	if (result != vk::Result::eSuccess)
		return result;

	SubmitInternal(submitInfos, fence);

	return vk::Result::eSuccess;
}

vk::Result VK_NAMESPACE::VK_CORE::WorkerQueue::Submit(vk::ArrayProxy<vk::Semaphore> signalSemaphores,
	vk::ArrayProxy<vk::CommandBuffer> buffers, vk::Fence fence, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(buffers);
	submitInfo.setSignalSemaphores(signalSemaphores);

	return Submit(submitInfo, fence, timeOut);
}

vk::Result VK_NAMESPACE::VK_CORE::WorkerQueue::Submit(vk::ArrayProxy<QueueWaitingPoint> waitPoints,
	vk::ArrayProxy<vk::Semaphore> signalSemaphores, vk::ArrayProxy<vk::CommandBuffer> buffers, vk::Fence fence, 
	std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	std::vector<vk::PipelineStageFlags> WaitStages;
	std::vector<vk::Semaphore> WaitSemaphores;
	WaitStages.reserve(waitPoints.size());

	for (const auto& waitPoint : waitPoints)
	{
		auto& stage = WaitStages.emplace_back();
		auto& waitSemaphore = WaitSemaphores.emplace_back();
		stage = waitPoint.WaitDst;
		waitSemaphore = waitPoint.WaitSemaphore;
	}

	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(buffers);
	submitInfo.setWaitDstStageMask(WaitStages);
	submitInfo.setWaitSemaphores(WaitSemaphores);
	submitInfo.setSignalSemaphores(signalSemaphores);

	return Submit(submitInfo, fence, timeOut);
}

vk::Result VK_NAMESPACE::VK_CORE::WorkerQueue::BindSparse(const vk::BindSparseInfo& bindSparseInfo,
	vk::Fence fence, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	std::unique_lock locker(mLock);

	auto result = WaitIdleAsync(timeOut.count(), fence);

	if (result != vk::Result::eSuccess)
		return result;

	BindSparseInternal(bindSparseInfo, fence);

	return result;
}

vk::Result VK_NAMESPACE::VK_CORE::WorkerQueue::PresentKHR(const vk::PresentInfoKHR& presentInfo) const
{
	std::scoped_lock locker(mLock);

	SaveThreadID();
	return mHandle.presentKHR(&presentInfo);
}

vk::Result VK_NAMESPACE::VK_CORE::WorkerQueue::WaitIdle()const
{
	std::scoped_lock locker(mLock);

	return WaitIdleAsync(0);
}

void VK_NAMESPACE::VK_CORE::WorkerQueue::SubmitInternal(vk::ArrayProxy<vk::SubmitInfo> submitInfos, vk::Fence fence) const
{
	SaveThreadID();

	if(fence)
		mDevice.resetFences(fence);

	mHandle.submit(submitInfos, fence);
}

void VK_NAMESPACE::VK_CORE::WorkerQueue::BindSparseInternal(const vk::BindSparseInfo& bindSparseInfo, vk::Fence fence) const
{
	SaveThreadID();

	if(fence)
		mDevice.resetFences(fence);

	mHandle.bindSparse(bindSparseInfo, fence);
}

vk::Result VK_NAMESPACE::VK_CORE::WorkerQueue::WaitIdleAsync(uint64_t timeout, vk::Fence fence) const
{
	if(fence)
		return mDevice.waitForFences(fence, VK_TRUE, timeout);

	mHandle.waitIdle();
	return vk::Result::eSuccess;
}

void VK_NAMESPACE::VK_CORE::WorkerQueue::SaveThreadID() const
{
	mThreadID = std::this_thread::get_id();
}
