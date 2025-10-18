#include "Core/vkpch.h"
#include "Process/WorkingClass.h"
#include "Process/Worker.h"

#include "Core/Utils/DeviceCreation.h"
#include "Core/MultiUniqueLock.h"

VK_NAMESPACE::Core::Worker VK_NAMESPACE::WorkingClass::FetchWorker(uint32_t familyIndex) const
{
	return { mDevice, &mWorkerFamilies.at(familyIndex) };
}

void VK_NAMESPACE::WorkingClass::WaitIdle() const
{
	std::vector<Core::WorkerLock*> mtxs = GetWorkerQueueLocks();

	// lock everything and then wait for the device to become idle
	UniqueLockRange<Core::WorkerLock> lockRng(mtxs);
	mDevice->waitIdle();
}

uint32_t VK_NAMESPACE::WorkingClass::FindOptimalQueueFamilyIndex(vk::QueueFlagBits flag) const
{
	const auto& Indices = mFamilyIndicesByCapabilities.at(flag);

	uint32_t OptimalFamilyIndex = -1;
	size_t MinSize = -1;

	for (auto index : Indices)
	{
		auto Caps = GetFamilyCapabilities(index);
		auto IndividualFlags = Core::Utils::BreakIntoIndividualFlagBits(Caps);

		if (MinSize > IndividualFlags.size())
		{
			OptimalFamilyIndex = index;
			MinSize = IndividualFlags.size();
		}
	}

	return OptimalFamilyIndex;
}

uint32_t VK_NAMESPACE::WorkingClass::GetWorkerCount(uint32_t FamilyIndex) const
{
	return static_cast<uint32_t>(mWorkerFamilies.at(FamilyIndex).Workers.size());
}
std::vector<VK_NAMESPACE::VK_CORE::WorkerLock*> VK_NAMESPACE::WorkingClass::GetWorkerQueueLocks() const
{
	std::vector<Core::WorkerLock*> workerLocks;

	for (const auto& [idx, family] : mWorkerFamilies)
	{
		workerLocks.insert(workerLocks.end(), family.mMutexes.begin(), family.mMutexes.end());
	}

	return workerLocks;
}

VK_NAMESPACE::WorkingClass::WorkingClass(
	const Core::QueueFamilyMap<std::vector<Core::Ref<Core::WorkerQueue>>>& queues, 
	const Core::QueueFamilyIndices& queueIndices, 
	const Core::QueueIndexMap& queueCaps,
	const std::vector<vk::QueueFamilyProperties> queueProps,
	Core::Ref<vk::Device> device) 

	: mWorkerFamilies(), mWorkerIndices(queueIndices),
	mFamilyIndicesByCapabilities(queueCaps), mDevice(device)

{
	for (const auto& queueFamily : queues)
	{
		mWorkerFamilies[queueFamily.first].Index = queueFamily.first;
		mWorkerFamilies[queueFamily.first].Workers = std::move(queueFamily.second);
		mWorkerFamilies[queueFamily.first].Capabilities = queueProps[queueFamily.first].queueFlags;
	}

	for (auto& workerFamilyInfo : mWorkerFamilies)
	{
		for (auto& worker : workerFamilyInfo.second.Workers)
		{
			worker->mFamilyInfo = &workerFamilyInfo.second;
			worker->mFamilyInfo->mMutexes.emplace_back(&worker->mLock);
		}
	}
}
