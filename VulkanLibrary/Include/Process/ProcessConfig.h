#pragma once
// Vulkan configuration and queue structures...
#include "../Core/Config.h"
#include "../Core/Ref.h"

VK_BEGIN
class WorkingClass;

VK_CORE_BEGIN

class WorkerQueue;

using QueueFamilyIndices = std::set<uint32_t>;
// QueueCapability --> Family Indices
using QueueIndexMap = std::map<vk::QueueFlagBits, std::set<uint32_t>>;
using CommandBufferSet = std::set<vk::CommandBuffer>;

template <typename T>
using QueueFamilyMap = std::map<uint32_t, T>;

using WorkerLock = std::mutex;

struct WorkerFamily
{
	uint32_t Index = -1;
	vk::QueueFlags Capabilities;

	mutable std::atomic_uint32_t mActiveQueue = 0;

	std::vector<WorkerLock*> mMutexes;
	std::vector<Core::Ref<WorkerQueue>> Workers;

	uint32_t Next() const { return mActiveQueue.exchange((mActiveQueue.load() + 1) % static_cast<uint32_t>(Workers.size())); }
};

struct QueueWaitingPoint
{
	vk::Semaphore WaitSemaphore{};
	vk::PipelineStageFlags WaitDst{};
};

struct CommandPoolData
{
	vk::CommandPool Handle;
	std::mutex Lock;

	CommandPoolData() = default;

	CommandPoolData(const CommandPoolData& Other)
		: Handle(Other.Handle) {}

	CommandPoolData& operator =(const CommandPoolData& Other)
	{ Handle = Other.Handle; }

	bool operator ==(const CommandPoolData& Other) const { return Handle == Other.Handle; }
};

struct CommandBufferDeleter
{
	void operator()(vk::CommandBuffer buffer) const
	{ Device->freeCommandBuffers(*CommandPool, buffer); }

	Core::Ref<vk::CommandPool> CommandPool;
	Core::Ref<vk::Device> Device;
};

VK_CORE_END
VK_END
