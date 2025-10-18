#pragma once
#include "ProcessConfig.h"
#include "Process/WorkerQueue.h"
#include "Process/Worker.h"

VK_BEGIN

class CommandPools;
class CommandBufferAllocator;

// keeps a reference to the cmd buf creator so 
// you don't have to worry about manually freeing it up
// be careful when using the command buffer outside of 
// the execution unit
struct ExecutionUnit
{
	vk::CommandBuffer CmdBufs;
	Core::Worker Worker;

	std::shared_ptr<CommandBufferAllocator> CmdAllocRef;

	VKLIB_API ~ExecutionUnit();
};

VKLIB_API vk::Result WaitForExecUnits(vk::ArrayProxy<ExecutionUnit> execUnits, bool waitAll = true, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

// Thread safe
class CommandBufferAllocator
{
public:
	CommandBufferAllocator() = default;
	VKLIB_API ~CommandBufferAllocator();

	VKLIB_API vk::CommandBuffer BeginOneTimeCommands(vk::CommandBufferLevel level =
		vk::CommandBufferLevel::ePrimary) const;

	VKLIB_API void EndOneTimeCommands(vk::CommandBuffer CmdBuffer, Core::Worker Executor) const;

	VKLIB_API vk::CommandBuffer Allocate(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const;
	VKLIB_API void Free(vk::CommandBuffer CmdBuffer) const;

	VKLIB_API ExecutionUnit CreateExecUnit(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const;
	VKLIB_API std::vector<ExecutionUnit> CreateExecUnits(uint32_t count, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const;

	void lock() { mCommandPool->Lock.lock(); }
	void unlock() { mCommandPool->Lock.unlock(); }
	bool try_lock() { return mCommandPool->Lock.try_lock(); }

	vk::CommandPool GetNativeHandle() const { return mCommandPool->Handle; }
	const CommandPools* GetPoolManager() const { return mParentReservoir; }
	uint32_t GetFamilyIndex() const { return mFamilyIndex; }

	bool operator ==(const CommandBufferAllocator& Other) const
		{ return mCommandPool == Other.mCommandPool; }

	bool operator !=(const CommandBufferAllocator& Other) const
		{ return !(*this == Other); }

	explicit operator bool() const { return static_cast<bool>(mCommandPool); }

private:
	// Fields...
	Core::Ref<Core::CommandPoolData> mCommandPool;
	uint32_t mFamilyIndex = -1;

	const CommandPools* mParentReservoir = nullptr;

	Core::Ref<vk::Device> mDevice;
	WorkingClassRef mWorkingClass;

private:
	// Debug...
#if _DEBUG
	mutable std::shared_ptr<Core::CommandBufferSet> mAllocatedInstances = nullptr;
#endif

	void AddInstanceDebug(vk::CommandBuffer CmdBuffer) const;
	void RemoveInstanceDebug(vk::CommandBuffer CmdBuffer) const;

	// Not necessary...
	void DestructionChecksDebug() const;

	friend class CommandPools;
};

// don't know if the queue family indices are guaranteed to consecutive
using CommandAllocatorMap = std::map<uint32_t, CommandBufferAllocator>;

// Thread safe
class CommandPools
{
public:
	CommandPools() = default;

	const CommandBufferAllocator& FindCmdPool(uint32_t familyIndex) const;
	const CommandBufferAllocator& operator[](uint32_t familyIndex) const { return FindCmdPool(familyIndex); }

	vk::CommandPoolCreateFlags GetCreationFlags() const { return mCreationFlags; }
	Core::Ref<vk::Device> GetDeviceHandle() const { return mDevice; }
	Core::QueueFamilyIndices GetQueueFamilyIndices() const { return mIndices; }

	bool operator ==(const CommandPools& Other) const
		{ return mCommandPools == Other.mCommandPools; }

	bool operator !=(const CommandPools& Other) const
		{ return !(*this == Other); }

	explicit operator bool() const { return !mCommandPools.empty(); }

private:
	CommandAllocatorMap mCommandPools;

	vk::CommandPoolCreateFlags mCreationFlags;
	Core::QueueFamilyIndices mIndices;
	Core::Ref<vk::Device> mDevice;
	WorkingClassRef mWorkingClass;

private:
	CommandPools(Core::Ref<vk::Device> device, const Core::QueueFamilyIndices& indices, vk::CommandPoolCreateFlags flags, WorkingClassRef workingClass);

	Core::Ref<Core::CommandPoolData> CreateCommandPool(uint32_t index);

private:
	void AssignTrackerDebug(CommandBufferAllocator& Allocator)const;
	void DoCopyChecksDebug(const CommandPools* Other) const;

	CommandBufferAllocator CreateAllocator(uint32_t familyIndex);

	friend class Context;
};


VK_END
