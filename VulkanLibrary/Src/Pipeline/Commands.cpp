#include "Core/vkpch.h"
#include "Process/Commands.h"

VK_NAMESPACE::ExecutionUnit::~ExecutionUnit()
{
	if (CmdAllocRef.use_count() == 1)
	{
		CmdAllocRef->Free(CmdBufs);
		return;
	}
}

VK_NAMESPACE::CommandBufferAllocator::~CommandBufferAllocator()
{
	DestructionChecksDebug();
}

vk::CommandBuffer VK_NAMESPACE::CommandBufferAllocator::BeginOneTimeCommands(
	vk::CommandBufferLevel level /*= vk::CommandBufferLevel::ePrimary*/) const
{
	vk::CommandBuffer buffer = Allocate(level);

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	buffer.begin(beginInfo);

	return buffer;
}

void VK_NAMESPACE::CommandBufferAllocator::EndOneTimeCommands(
	vk::CommandBuffer CmdBuffer, Core::Worker workers) const
{
	CmdBuffer.end();

	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers(CmdBuffer);

	workers.Dispatch(submitInfo);
	workers.WaitIdle();

	Free(CmdBuffer);
}

vk::CommandBuffer VK_NAMESPACE::CommandBufferAllocator::Allocate(
	vk::CommandBufferLevel level /*= vk::CommandBufferLevel::ePrimary*/) const
{
	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.setCommandPool(mCommandPool->Handle);
	allocInfo.setCommandBufferCount(1);
	allocInfo.setLevel(level);

	vk::CommandBuffer CmdBuffer = mDevice->allocateCommandBuffers(allocInfo).front();
	AddInstanceDebug(CmdBuffer);

	return CmdBuffer;
}

void VK_NAMESPACE::CommandBufferAllocator::Free(vk::CommandBuffer CmdBuffer) const
{
	RemoveInstanceDebug(CmdBuffer);
	mDevice->freeCommandBuffers(mCommandPool->Handle, CmdBuffer);
}

VK_NAMESPACE::ExecutionUnit VK_NAMESPACE::CommandBufferAllocator::CreateExecUnit(vk::CommandBufferLevel level /*= vk::CommandBufferLevel::ePrimary*/) const
{
	ExecutionUnit execUnit;
	execUnit.CmdBufs = Allocate(level);
	execUnit.Worker = mWorkingClass->FetchWorker(mFamilyIndex);
	execUnit.CmdAllocRef = std::make_shared<CommandBufferAllocator>(*this);

	return execUnit;
}

std::vector<VK_NAMESPACE::ExecutionUnit> VK_NAMESPACE::CommandBufferAllocator::CreateExecUnits(uint32_t count, vk::CommandBufferLevel level /*= vk::CommandBufferLevel::ePrimary*/) const
{
	std::vector<ExecutionUnit> execUnits(count);

	for (uint32_t i = 0; i < execUnits.size(); i++)
	{
		execUnits[i] = CreateExecUnit(level);
	}

	return execUnits;
}

void VK_NAMESPACE::CommandBufferAllocator::AddInstanceDebug(vk::CommandBuffer CmdBuffer) const
{
#if _DEBUG
	mAllocatedInstances->insert(CmdBuffer);
#endif
}

void VK_NAMESPACE::CommandBufferAllocator::RemoveInstanceDebug(vk::CommandBuffer CmdBuffer) const
{
#if _DEBUG
	auto found = mAllocatedInstances->find(CmdBuffer);
	_STL_ASSERT(found != mAllocatedInstances->end(), "Trying to free an instance of 'vk::CommandBuffer' from "
		"an allocator (a 'vkLib::CommandBufferAllocator' instance) that did not create the buffer\n"
		"You must free the buffer from where it was created");
	mAllocatedInstances->erase(CmdBuffer);
#endif
}

void VK_NAMESPACE::CommandBufferAllocator::DestructionChecksDebug() const
{
#if _DEBUG
	if (mAllocatedInstances.use_count() != 1)
		return;

	_STL_ASSERT(mAllocatedInstances->empty(),
		"All instances of VK_NAMESPACE::CommandBufferAllocator's along its parent CommandReservoir "
		"were deleted before freeing its vk::CommandBuffers");
#endif
}

VK_NAMESPACE::CommandPools::CommandPools(Core::Ref<vk::Device> device, const Core::QueueFamilyIndices& indices, vk::CommandPoolCreateFlags flags, WorkingClassRef workingClass)
	: mIndices(indices), mCreationFlags(flags), mDevice(device), mWorkingClass(workingClass)
{
	for (auto index : mIndices)
		mCommandPools[index] = CreateAllocator(index);
}

VK_NAMESPACE::Core::Ref<VK_NAMESPACE::Core::CommandPoolData> 
	VK_NAMESPACE::CommandPools::CreateCommandPool(uint32_t index)
{
	vk::CommandPoolCreateInfo createInfo{};
	createInfo.setFlags(mCreationFlags);
	createInfo.setQueueFamilyIndex(index);

	auto Handle = mDevice->createCommandPool(createInfo);
	auto Device = mDevice;

	Core::CommandPoolData Pool;
	Pool.Handle = Handle;

	return Core::CreateRef<Core::CommandPoolData>([Device](Core::CommandPoolData& PoolData)
	{ Device->destroyCommandPool(PoolData.Handle); }, Pool);
}

const VK_NAMESPACE::CommandBufferAllocator& VK_NAMESPACE::CommandPools::FindCmdPool(uint32_t familyIndex) const
{
	_STL_ASSERT(mIndices.find(familyIndex) != mIndices.end(),
		"Invalid queue family index passed into "
		"'CommandBufferAllocator::FindCmdPool(uint32_t, bool)'!");

	return mCommandPools.at(familyIndex);
}

void VK_NAMESPACE::CommandPools::AssignTrackerDebug(CommandBufferAllocator& Allocator) const
{
#if _DEBUG
	Allocator.mAllocatedInstances = std::make_shared<std::set<vk::CommandBuffer>>();
#endif
}

void VK_NAMESPACE::CommandPools::DoCopyChecksDebug(const CommandPools* Other) const
{
	_STL_ASSERT(mCreationFlags == Other->mCreationFlags,
		"Trying to copy CommandPoolManager's with different creation flags!");

	_STL_ASSERT(*mDevice == *Other->mDevice,
		"Trying to copy CommandPoolManager's created from different devices!");
}

VK_NAMESPACE::CommandBufferAllocator VK_NAMESPACE::CommandPools::CreateAllocator(uint32_t familyIndex)
{
	CommandBufferAllocator Allocator{};
	Allocator.mDevice = mDevice;
	Allocator.mCommandPool = CreateCommandPool(familyIndex);
	Allocator.mFamilyIndex = familyIndex;
	Allocator.mParentReservoir = this;
	Allocator.mWorkingClass = mWorkingClass;

	AssignTrackerDebug(Allocator);

	return Allocator;
}

vk::Result VK_NAMESPACE::WaitForExecUnits(vk::ArrayProxy<ExecutionUnit> execUnits, bool waitAll /*= true*/, std::chrono::nanoseconds timeout /*= std::chrono::nanoseconds::max()*/)
{
	if (execUnits.empty())
		return vk::Result::eSuccess;

	std::vector<vk::Fence> fences;
	fences.reserve(execUnits.size());

	for (auto unit : execUnits)
		fences.emplace_back(unit.Worker.GetFence());

	// assuming all the workers share the same device
	return execUnits.front().Worker.GetDeviceHandle().waitForFences(fences, waitAll, timeout.count());
}
