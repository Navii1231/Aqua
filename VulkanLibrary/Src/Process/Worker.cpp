#include "Core/vkpch.h"
#include "Process/Worker.h"

uint32_t VK_NAMESPACE::VK_CORE::Worker::Enqueue(vk::ArrayProxy<vk::SubmitInfo> submitInfo, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	return SubmitRange(submitInfo, timeOut);
}

uint32_t VK_NAMESPACE::VK_CORE::Worker::Enqueue(vk::ArrayProxy<vk::CommandBuffer> cmdBuffer, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(cmdBuffer);

	return Enqueue(submitInfo, timeOut);
}

uint32_t VK_NAMESPACE::VK_CORE::Worker::Enqueue(vk::ArrayProxy<vk::Semaphore> signalSemaphores, vk::ArrayProxy<vk::CommandBuffer> buffers, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(buffers);
	submitInfo.setSignalSemaphores(signalSemaphores);

	return Enqueue(submitInfo, timeOut);
}

uint32_t VK_NAMESPACE::VK_CORE::Worker::Enqueue(vk::ArrayProxy<QueueWaitingPoint> waitPoints, vk::ArrayProxy<vk::Semaphore> signalSemaphores, vk::ArrayProxy<vk::CommandBuffer> buffers, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
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

	return Enqueue(submitInfo, timeOut);
}

vk::Result VK_NAMESPACE::VK_CORE::Worker::WaitIdle(std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/) const
{
	return mDevice->waitForFences(*mFence, VK_TRUE, timeOut.count());
}

bool VK_NAMESPACE::VK_CORE::Worker::IsFree() const
{
	// checking if the fence is available
	return WaitIdle(std::chrono::seconds(0)) == vk::Result::eSuccess;
}

vk::Result VK_NAMESPACE::VK_CORE::Worker::WaitForQueues(size_t beginSpan, size_t endSpan, 
	bool waitForAll, std::chrono::nanoseconds timeOut) const
{
	// wait for all of fences but set the variable 'waitAll' to VK_FALSE or VK_TRUE
	// this will make sure the function returns the moment any fence is signaled
	// well, now we can poll all the workers to check out which one was freed

	for (auto queue : mFamilyData->Workers)
	{
		auto result = queue->WaitIdle();

		if (result != vk::Result::eSuccess)
			return result;
	}

	return vk::Result::eSuccess;
}

uint32_t VK_NAMESPACE::VK_CORE::Worker::SubmitRange(const vk::ArrayProxy<vk::SubmitInfo>& submitInfos, std::chrono::nanoseconds timeOut) const
{
	auto workers = GetActiveWorkers();

	// iterating to the next index and submitting the work to the GPU
	uint32_t currWorker = mFamilyData->Next() % static_cast<uint32_t>(workers.size());

	auto result = workers[currWorker]->Submit(submitInfos, *mFence, timeOut);

	if (result != vk::Result::eSuccess)
		return std::numeric_limits<uint32_t>::max();

	return currWorker;
}

std::span<const VK_NAMESPACE::VK_CORE::Ref<VK_NAMESPACE::VK_CORE::WorkerQueue>> VK_NAMESPACE::VK_CORE::Worker::GetActiveWorkers() const
{
	std::span<const Ref<WorkerQueue>> workers(mFamilyData->Workers.begin(), mFamilyData->Workers.end());

	return workers;
}

vk::Result VK_NAMESPACE::WaitForWorkers(vk::ArrayProxy<Core::Worker> workers, 
	bool waitAll /*= true*/, std::chrono::nanoseconds timeout /*= std::chrono::nanoseconds::max()*/)
{
	if (workers.empty())
		return vk::Result::eSuccess;

	std::vector<vk::Fence> fences;
	fences.reserve(workers.size());

	for (auto worker : workers)
		fences.emplace_back(worker.GetFence());

	// assuming all the workers share the same device
	return workers.front().mDevice->waitForFences(fences, waitAll, timeout.count());
}

