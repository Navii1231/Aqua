#include "Core/vkpch.h"
#include "Memory/GenericBuffer.h"

void VK_NAMESPACE::Buffer<bool>::UnmapMemory() const
{
	mChunk.Device->unmapMemory(mChunk.BufferHandles->Memory);

	if (mChunk.BufferHandles->Config.MemProps & vk::MemoryPropertyFlagBits::eHostCoherent)
		return;

	// Synchronize manually in case the underlying memory isn't HostCoherent
	vk::MappedMemoryRange range{};
	range.setMemory(mChunk.BufferHandles->Memory);
	range.setOffset(0);
	range.setSize(mChunk.BufferHandles->BufferSize);

	mChunk.Device->flushMappedMemoryRanges(range);
}

void VK_NAMESPACE::Buffer<bool>::InsertMemoryBarrier(vk::CommandBuffer commandBuffer, const MemoryBarrierInfo& pipelineBarrierInfo)
{
	vk::BufferMemoryBarrier barrier;
	barrier.setBuffer(mChunk.BufferHandles->Handle);
	barrier.setSrcAccessMask(pipelineBarrierInfo.SrcAccessMasks);
	barrier.setDstAccessMask(pipelineBarrierInfo.DstAccessMasks);
	barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
	barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
	barrier.setSize(VK_WHOLE_SIZE);

	commandBuffer.pipelineBarrier(pipelineBarrierInfo.SrcPipeleinStages, pipelineBarrierInfo.DstPipelineStages,
		pipelineBarrierInfo.DependencyFlags, nullptr, barrier, nullptr);
}

void VK_NAMESPACE::Buffer<bool>::TransferOwnership(uint32_t DstQueueFamilyIndex) const
{
	uint32_t& SrcOwner = mChunk.BufferHandles->Config.ResourceOwner;

	if (SrcOwner == DstQueueFamilyIndex)
		return;

	vk::Semaphore bufferReleased = mChunk.Device->createSemaphore({});

	ReleaseBuffer(DstQueueFamilyIndex, bufferReleased);
	AcquireBuffer(DstQueueFamilyIndex, bufferReleased);

	mChunk.Device->destroySemaphore(bufferReleased);

	SrcOwner = DstQueueFamilyIndex;
}

void VK_NAMESPACE::Buffer<bool>::TransferOwnership(vk::QueueFlagBits optimizedCaps) const
{
	uint32_t DstQueueFamily = mWorkingClass->FindOptimalQueueFamilyIndex(optimizedCaps);
	TransferOwnership(DstQueueFamily);
}

bool VK_NAMESPACE::Buffer<bool>::IsMappable()
{
	auto memProps = mChunk.BufferHandles->Config.MemProps;
	return memProps == vk::MemoryPropertyFlagBits::eHostCoherent ||
		memProps == vk::MemoryPropertyFlagBits::eHostVisible;
}

void VK_NAMESPACE::Buffer<bool>::Reserve(size_t NewCap)
{
	if (NewCap > mChunk.BufferHandles->Config.DeviceSize)
		ScaleCapacityWithoutLoss(NewCap);
}

void VK_NAMESPACE::Buffer<bool>::Resize(size_t NewSize)
{
	Reserve(NewSize);
	mChunk.BufferHandles->BufferSize = NewSize;
}

void VK_NAMESPACE::Buffer<bool>::ScaleCapacity(size_t NewSize)
{
	mChunk.BufferHandles->Config.DeviceSize = NewSize;
	Core::Buffer NewBuffer = Core::Utils::CreateBuffer(mChunk.BufferHandles->Config);

	auto Device = mChunk.Device;

	mChunk.BufferHandles.SetValue(NewBuffer);
}

void VK_NAMESPACE::Buffer<bool>::ScaleCapacityWithoutLoss(size_t NewSize)
{
	mChunk.BufferHandles->Config.DeviceSize = NewSize;
	Core::Buffer NewBuffer = Core::Utils::CreateBuffer(mChunk.BufferHandles->Config);

	vk::BufferCopy CopyRegion{};
	CopyRegion.setSize(mChunk.BufferHandles->BufferSize);

	// Recover the past
	CopyGPU(NewBuffer, *mChunk.BufferHandles, CopyRegion);

	mChunk.BufferHandles.SetValue(NewBuffer);
}

void VK_NAMESPACE::Buffer<bool>::CopyGPU(Core::Buffer& DstBuffer, const Core::Buffer& SrcBuffer,
	const vk::ArrayProxy<vk::BufferCopy>& CopyRegions)
{
	if (CopyRegions.front().size == 0)
		return;

	uint32_t DstOwner = DstBuffer.Config.ResourceOwner;

	InvokeOneTimeProcess(DstOwner, [DstOwner, &SrcBuffer, &DstBuffer, &CopyRegions, this]
	(vk::CommandBuffer CmdBuffer)
		{
			CmdBuffer.copyBuffer(SrcBuffer.Handle, DstBuffer.Handle, CopyRegions);
		});
}

inline void VK_NAMESPACE::Buffer<bool>::ReleaseBuffer(uint32_t dstIndex, vk::Semaphore bufferReleased) const
{
	uint32_t OwnerIndex = mChunk.BufferHandles->Config.ResourceOwner;

	InvokeProcess(OwnerIndex, [bufferReleased, OwnerIndex, dstIndex, this](
		vk::CommandBuffer CmdBuffer)->vk::SubmitInfo
		{
			Core::BufferOwnershipTransferInfo transferInfo{};

			transferInfo.Buffer = *mChunk.BufferHandles;
			transferInfo.CmdBuffer = CmdBuffer;
			transferInfo.SrcFamilyIndex = OwnerIndex;
			transferInfo.DstFamilyIndex = dstIndex;

			Core::Utils::FillBufferReleaseMasks(transferInfo,
				mWorkingClass->GetFamilyCapabilities(OwnerIndex));


			CmdBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

			Core::Utils::RecordBufferTransferBarrier(transferInfo);

			CmdBuffer.end();

			vk::SubmitInfo submitInfo{};
			submitInfo.setCommandBuffers(CmdBuffer);
			submitInfo.setSignalSemaphores(bufferReleased);

			return submitInfo;
		});
}

inline void VK_NAMESPACE::Buffer<bool>::AcquireBuffer(uint32_t acquiringFamily, vk::Semaphore bufferReleased) const
{
	uint32_t OwnerIndex = mChunk.BufferHandles->Config.ResourceOwner;
	uint32_t DstIndex = acquiringFamily;

	InvokeProcess(DstIndex, [bufferReleased, OwnerIndex, DstIndex, this](
		vk::CommandBuffer CmdBuffer)->vk::SubmitInfo
		{
			Core::BufferOwnershipTransferInfo transferInfo{};

			transferInfo.Buffer = *mChunk.BufferHandles;
			transferInfo.CmdBuffer = CmdBuffer;
			transferInfo.SrcFamilyIndex = OwnerIndex;
			transferInfo.DstFamilyIndex = DstIndex;

			Core::Utils::FillBufferAcquireMasks(transferInfo,
				mWorkingClass->GetFamilyCapabilities(DstIndex));


			CmdBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

			Core::Utils::RecordBufferTransferBarrier(transferInfo);

			CmdBuffer.end();

			vk::PipelineStageFlags waitFlags[] = { vk::PipelineStageFlagBits::eTopOfPipe };

			vk::SubmitInfo submitInfo{};
			submitInfo.setCommandBuffers(CmdBuffer);
			submitInfo.setWaitSemaphores(bufferReleased);
			submitInfo.setWaitDstStageMask(waitFlags);

			return submitInfo;
		});
}

VK_NAMESPACE::Buffer<bool> VK_NAMESPACE::Buffer<bool>::StageBuffer(size_t count)
{
	Buffer buffer{};

	Core::BufferConfig hostConfig = mChunk.BufferHandles->Config;
	hostConfig.MemProps = vk::MemoryPropertyFlagBits::eHostCoherent;

	auto device = mChunk.Device;

	buffer.mChunk.BufferHandles = Core::CreateRef<Core::Buffer>(
		[device](Core::Buffer& buffer)
	{
		device->destroyBuffer(buffer.Handle);
		device->freeMemory(buffer.Memory);
	}, Core::Utils::CreateBuffer(hostConfig));

	buffer.mChunk.Device = device;

	return buffer;
}

void VK_NAMESPACE::Buffer<bool>::MakeHollow()
{
	mChunk.BufferHandles.Reset();
	mChunk.BufferHandles->BufferSize = 0;
}

void VK_NAMESPACE::Buffer<bool>::ShrinkToFit()
{
	if (mChunk.BufferHandles->BufferSize != mChunk.BufferHandles->Config.DeviceSize)
		Resize(mChunk.BufferHandles->BufferSize);
}
