#pragma once
#include "WorkerQueue.h"

VK_BEGIN
VK_CORE_BEGIN

class Worker;

VK_CORE_END

// should be inside the core, it's not directly visible to the client
class WorkingClass
{
public:
	// Executor for submitting work into queues asynchronously
	VKLIB_API Core::Worker FetchWorker(uint32_t familyIndex) const;

	VKLIB_API void WaitIdle() const;

	VKLIB_API uint32_t FindOptimalQueueFamilyIndex(vk::QueueFlagBits flag) const;
	vk::QueueFlags GetFamilyCapabilities(uint32_t index) const
	{
		return mWorkerFamilies.at(index).Capabilities;
	}

	// Getter for info...
	VKLIB_API uint32_t GetWorkerCount(uint32_t familyIndex) const;

	VKLIB_API std::vector<Core::WorkerLock*> GetWorkerQueueLocks() const;

	VKLIB_API Core::QueueFamilyIndices GetPresentQueueFamilyIndices() const;

	const Core::WorkerFamily& GetWorkerFamily(uint32_t familyIndex) const { return mWorkerFamilies.at(familyIndex); }
	const Core::QueueFamilyIndices& GetWorkerFamilyIndices() const { return mWorkerIndices; }
	const Core::QueueIndexMap& GetFamilyIndicesByCapabilityMap() const { return mFamilyIndicesByCapabilities; }

private:
	Core::QueueFamilyMap<Core::WorkerFamily> mWorkerFamilies;

	Core::QueueFamilyIndices mWorkerIndices;
	Core::QueueIndexMap mFamilyIndicesByCapabilities;

	Core::Ref<vk::Device> mDevice;

private:
	WorkingClass(const Core::QueueFamilyMap<std::vector<Core::Ref<Core::WorkerQueue>>>& queues,
		const Core::QueueFamilyIndices& queueIndices,
		const Core::QueueIndexMap& queueCaps,
		const std::vector<vk::QueueFamilyProperties> queueProps,
		Core::Ref<vk::Device> device);

	WorkingClass(const WorkingClass&) = delete;
	WorkingClass& operator =(const WorkingClass&) = delete;

	friend class Context;
};

using WorkingClassRef = std::shared_ptr<const WorkingClass>;

VK_END
