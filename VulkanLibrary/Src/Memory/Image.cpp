#include "Core/vkpch.h"
#include "Memory/Image.h"
#include "Memory/ImageView.h"

#include "Memory/ResourcePool.h"

VK_NAMESPACE::ImageView VK_NAMESPACE::Image::
	CreateImageView(const ImageViewCreateInfo& info) const
{
	if (info == mChunk->ImageHandles.IdentityViewInfo)
		return GetIdentityImageView();

	auto Device = mChunk->Device;
	auto ImageHandles = mChunk->ImageHandles;

	ImageViewData viewData;
	viewData.Info = info;
	viewData.View = Core::Utils::CreateImageView(*mChunk->Device, mChunk->ImageHandles.Handle, info);
	viewData.Size = GetSize();

	ImageView view;

	view.mData = Core::CreateRef<ImageViewData>([Device, ImageHandles](ImageViewData& data)
	{ Device->destroyImageView(data.View); }, viewData);

	view.mParent = *this;

	return view;
}

void VK_NAMESPACE::Image::Blit(const Image& src, ImageBlitInfo imageBlitInfo)
{
	uint32_t Owner = mChunk->ImageHandles.Config.ResourceOwner;
	auto OwnerCaps = mWorkingClass->GetFamilyCapabilities(Owner);

	glm::vec2 srcSize = src.GetSize();

	imageBlitInfo = CheckBounds(imageBlitInfo, srcSize);

	InvokeOneTimeProcess(Owner, [this, OwnerCaps, &src, &imageBlitInfo](vk::CommandBuffer CmdBuffer)
		{
			RecordTransitionLayoutInternal(vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eTransfer,
				mChunk->ImageHandles.Config.CurrLayout, mChunk->ImageHandles.Config.PrevStage,
				CmdBuffer, OwnerCaps);

			src.RecordTransitionLayoutInternal(vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits::eTransfer,
				src.mChunk->ImageHandles.Config.CurrLayout, src.mChunk->ImageHandles.Config.PrevStage,
				CmdBuffer, OwnerCaps);

			RecordBlitInternal(imageBlitInfo, vk::ImageLayout::eTransferDstOptimal,
				src, vk::ImageLayout::eTransferSrcOptimal, CmdBuffer);

			RecordTransitionLayoutInternal(mChunk->ImageHandles.Config.CurrLayout,
				mChunk->ImageHandles.Config.PrevStage,
				vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eTransfer, CmdBuffer, OwnerCaps);

			src.RecordTransitionLayoutInternal(src.mChunk->ImageHandles.Config.CurrLayout,
				src.mChunk->ImageHandles.Config.PrevStage,
				vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits::eTransfer, CmdBuffer, OwnerCaps);
		});
}

void VK_NAMESPACE::Image::TransitionLayout(vk::ImageLayout NewLayout, vk::PipelineStageFlags usageStage) const
{
	_STL_ASSERT(NewLayout != vk::ImageLayout::eUndefined && NewLayout != vk::ImageLayout::ePreinitialized,
		"Can't transition image layout to Undefined or Preinitialized format!");

	if (mChunk->ImageHandles.Config.CurrLayout == NewLayout)
		return;

	uint32_t Owner = mChunk->ImageHandles.Config.ResourceOwner;

	auto QueueCaps = mWorkingClass->GetFamilyCapabilities(Owner);

	InvokeOneTimeProcess(Owner, [this, NewLayout, usageStage, &QueueCaps](vk::CommandBuffer CmdBuffer)
		{
			RecordTransitionLayoutInternal(NewLayout, usageStage,
				mChunk->ImageHandles.Config.CurrLayout, mChunk->ImageHandles.Config.PrevStage,
				CmdBuffer, QueueCaps);
		});

	mChunk->ImageHandles.Config.CurrLayout = NewLayout;
	mChunk->ImageHandles.Config.PrevStage = usageStage;

	mChunk->ImageHandles.RecordedLayout = { mChunk->ImageHandles.Config.CurrLayout,
		mChunk->ImageHandles.Config.PrevStage };
}

void VK_NAMESPACE::Image::BeginCommands(vk::CommandBuffer commandBuffer) const
{
	DefaultBegin(commandBuffer);

	mChunk->ImageHandles.RecordedLayout = { mChunk->ImageHandles.Config.CurrLayout,
		mChunk->ImageHandles.Config.PrevStage };
}

void VK_NAMESPACE::Image::RecordBlit(const Image& src, ImageBlitInfo blitInfo, bool restoreOriginalLayout /*= true*/) const
{
	auto DstLayout = mChunk->ImageHandles.RecordedLayout;
	auto SrcLayout = src.mChunk->ImageHandles.RecordedLayout;

	auto OwnerCaps = mWorkingClass->GetFamilyCapabilities(
		mChunk->ImageHandles.Config.ResourceOwner);

	blitInfo = CheckBounds(blitInfo, src.GetSize());

	RecordTransitionLayout(vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eTransfer);
	src.RecordTransitionLayoutInternal(
		vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits::eTransfer,
		SrcLayout.Layout, SrcLayout.Stages, mWorkingCommandBuffer, OwnerCaps);

	RecordBlitInternal(blitInfo, vk::ImageLayout::eTransferDstOptimal,
		src, vk::ImageLayout::eTransferSrcOptimal, mWorkingCommandBuffer);

	if(restoreOriginalLayout)
		RecordTransitionLayout(DstLayout.Layout, DstLayout.Stages);

	src.RecordTransitionLayoutInternal(SrcLayout.Layout, SrcLayout.Stages,
		vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits::eTransfer,
		mWorkingCommandBuffer, OwnerCaps);
}

void VK_NAMESPACE::Image::RecordTransitionLayout(vk::ImageLayout newLayout, 
	vk::PipelineStageFlags usageStage) const
{
	if (mChunk->ImageHandles.RecordedLayout.Layout == newLayout)
		return;

	auto OwnerCaps = mWorkingClass->GetFamilyCapabilities(mChunk->ImageHandles.Config.ResourceOwner);

	RecordTransitionLayoutInternal(newLayout, usageStage,
		mChunk->ImageHandles.RecordedLayout.Layout, mChunk->ImageHandles.RecordedLayout.Stages,
		mWorkingCommandBuffer, OwnerCaps);

	mChunk->ImageHandles.RecordedLayout = { newLayout, usageStage };
}

void VK_NAMESPACE::Image::EndCommands() const
{
	RecordTransitionLayout(mChunk->ImageHandles.Config.CurrLayout,
		mChunk->ImageHandles.Config.PrevStage);

	DefaultEnd();
}

void VK_NAMESPACE::Image::TransferOwnership(uint32_t queueFamilyIndex) const
{
	if (mChunk->ImageHandles.Config.ResourceOwner == queueFamilyIndex)
		return;

	vk::ImageLayout PrevLayout = mChunk->ImageHandles.Config.CurrLayout;
	vk::PipelineStageFlags PrevStage = mChunk->ImageHandles.Config.PrevStage;

	TransitionLayout(vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eTopOfPipe);

	ReleaseImage(queueFamilyIndex);
	AcquireImage(queueFamilyIndex);

	TransitionLayout(PrevLayout, PrevStage);
}

void VK_NAMESPACE::Image::TransferOwnership(vk::QueueFlagBits Cap) const
{
	uint32_t DstQueueFamily = mWorkingClass->FindOptimalQueueFamilyIndex(Cap);
	TransferOwnership(DstQueueFamily);
}

std::vector<vk::ImageSubresourceRange> VK_NAMESPACE::Image::GetSubresourceRanges() const
{
	/* TODO: Hard coding it now, it needs to be addressed as soon as possible!!! */

	vk::ImageSubresourceRange Subresource = mChunk->ImageHandles.IdentityViewInfo.Subresource;

	/* ------------------------------------------------------------------------- */

	return { Subresource };
}

std::vector<vk::ImageSubresourceLayers> VK_NAMESPACE::Image::GetSubresourceLayers() const
{
	vk::ImageSubresourceLayers Subresource;

	/* TODO: Hard coding it now, it needs to be addressed as soon as possible!!! */

	Subresource.aspectMask = mChunk->ImageHandles.IdentityViewInfo.Subresource.aspectMask;
	Subresource.mipLevel = 0;
	Subresource.layerCount = 1;
	Subresource.baseArrayLayer = 0;

	/* ------------------------------------------------------------------------- */

	return { Subresource };
}

VK_NAMESPACE::ImageView VK_NAMESPACE::Image::GetIdentityImageView() const
{
	auto Device = mChunk->Device;
	auto ImageHandles = mChunk->ImageHandles;

	ImageViewData viewData;
	viewData.Info = mChunk->ImageHandles.IdentityViewInfo;
	viewData.View = mChunk->ImageHandles.IdentityView;
	viewData.Size = GetSize();

	ImageView view;

	view.mData = Core::CreateRef<ImageViewData>([Device, ImageHandles](ImageViewData& data) {}, viewData);
	view.mParent = *this;

	return view;
}

VK_NAMESPACE::GenericBuffer VK_NAMESPACE::Image::GetImageBuffer() const
{
	auto extent = mChunk->ImageHandles.Config.Extent;
	auto memoryReq = mChunk->ImageHandles.MemReq;

	GenericBuffer buffer = mChunk->RscPool->CreateGenericBuffer(vk::BufferUsageFlagBits::eStorageBuffer, memoryReq.size);

	vk::BufferImageCopy CopyRegion{};
	CopyRegion.setBufferOffset(0);
	CopyRegion.setBufferImageHeight(0);
	CopyRegion.setBufferRowLength(0);

	CopyRegion.setImageOffset({ 0, 0, 0 });
	CopyRegion.setImageExtent(mChunk->ImageHandles.Config.Extent);
	CopyRegion.setImageSubresource(GetSubresourceLayers().front());

	CopyToBuffer(mChunk->ImageHandles, buffer.GetNativeHandles(), CopyRegion);

	return buffer;
}

void VK_NAMESPACE::Image::RecordTransitionLayoutInternal(vk::ImageLayout newLayout, 
	vk::PipelineStageFlags newStages, vk::ImageLayout oldLayout, 
	vk::PipelineStageFlags oldStages, vk::CommandBuffer CmdBuffer, vk::QueueFlags QueueCaps) const
{
	if (newLayout == oldLayout)
		return;

	Core::ImageLayoutTransitionInfo transitionInfo{ mChunk->ImageHandles };

	transitionInfo.OldLayout = oldLayout;
	transitionInfo.NewLayout = newLayout;
	transitionInfo.CmdBuffer = CmdBuffer;

	transitionInfo.Subresource = GetSubresourceRanges().front();

	transitionInfo.SrcStageFlags = oldStages;
	transitionInfo.DstStageFlags = newStages;

	Core::Utils::FillImageTransitionLayoutMasks(transitionInfo, QueueCaps, transitionInfo.OldLayout,
		newLayout, transitionInfo.SrcStageFlags, newStages);
	Core::Utils::RecordImageLayoutTransition(transitionInfo);
}

void VK_NAMESPACE::Image::RecordBlitInternal(const ImageBlitInfo& imageBlitInfo, vk::ImageLayout dstLayout,
	const Image& src, vk::ImageLayout srcLayout, vk::CommandBuffer CmdBuffer) const
{
	vk::ImageBlit blitInfo{};

	blitInfo.srcOffsets[0] = vk::Offset3D(imageBlitInfo.SrcBeginRegion.x, imageBlitInfo.SrcBeginRegion.y, 0);
	blitInfo.srcOffsets[1] = vk::Offset3D(imageBlitInfo.SrcEndRegion.x, imageBlitInfo.SrcEndRegion.y, 1);

	blitInfo.dstOffsets[0] = vk::Offset3D(imageBlitInfo.DstBeginRegion.x, imageBlitInfo.DstBeginRegion.y, 0);
	blitInfo.dstOffsets[1] = vk::Offset3D(imageBlitInfo.DstEndRegion.x, imageBlitInfo.DstEndRegion.y, 1);

	blitInfo.srcSubresource = src.GetSubresourceLayers().front();
	blitInfo.dstSubresource = GetSubresourceLayers().front();

	CmdBuffer.blitImage(src.mChunk->ImageHandles.Handle, srcLayout, mChunk->ImageHandles.Handle,
		dstLayout, blitInfo, imageBlitInfo.Filter);
}

void VK_NAMESPACE::Image::CopyFromImage(Core::Image& DstImage, const Core::Image& SrcImage,
	const vk::ArrayProxy<vk::ImageCopy>& CopyRegions) const
{
	uint32_t Owner = mChunk->ImageHandles.Config.ResourceOwner;
	auto OwnerCaps = mWorkingClass->GetFamilyCapabilities(Owner);

	InvokeOneTimeProcess(Owner, [this, &SrcImage, &DstImage, &CopyRegions](vk::CommandBuffer CmdBuffer)
		{
			CmdBuffer.copyImage(SrcImage.Handle, SrcImage.Config.CurrLayout,
				DstImage.Handle, DstImage.Config.CurrLayout, CopyRegions);
		});
}

void VK_NAMESPACE::Image::CopyFromBuffer(Core::Image& DstImage, 
	const Core::Buffer& SrcBuffer, const vk::ArrayProxy<vk::BufferImageCopy>& CopyRegions) const
{
	InvokeOneTimeProcess(mChunk->ImageHandles.Config.ResourceOwner, [this, &SrcBuffer, &DstImage, &CopyRegions](vk::CommandBuffer CmdBuffer)
	{
		CmdBuffer.copyBufferToImage(SrcBuffer.Handle, DstImage.Handle,
			DstImage.Config.CurrLayout, CopyRegions);
	});
}

void VK_NAMESPACE::Image::CopyToBuffer(Core::Image& srcImage, const Core::Buffer& dstBuf, const vk::ArrayProxy<vk::BufferImageCopy>& copyRegions) const
{
	InvokeOneTimeProcess(mChunk->ImageHandles.Config.ResourceOwner, [this, &srcImage, &dstBuf, &copyRegions](vk::CommandBuffer buffer)
		{
			buffer.copyImageToBuffer(srcImage.Handle, srcImage.Config.CurrLayout, dstBuf.Handle, copyRegions);
		});
}

void VK_NAMESPACE::Image::ReleaseImage(uint32_t dstQueueFamily) const
{
	uint32_t Owner = mChunk->ImageHandles.Config.ResourceOwner;
	auto OwnerCaps = mWorkingClass->GetFamilyCapabilities(Owner);

	InvokeOneTimeProcess(Owner, [this, &OwnerCaps, Owner, dstQueueFamily](vk::CommandBuffer CmdBuffer)
		{
			Core::ImageLayoutTransitionInfo transferInfo{ mChunk->ImageHandles };
			transferInfo.CmdBuffer = CmdBuffer;
			transferInfo.OldLayout = mChunk->ImageHandles.Config.CurrLayout;

			transferInfo.SrcStageFlags = mChunk->ImageHandles.Config.PrevStage;
			transferInfo.DstStageFlags = vk::PipelineStageFlagBits::eTransfer;

			Core::Utils::FillImageReleaseMasks(transferInfo, OwnerCaps,
				transferInfo.OldLayout, transferInfo.SrcStageFlags);
			transferInfo.Subresource = GetSubresourceRanges().front();

			Core::Utils::RecordImageLayoutTransition(transferInfo, Owner, dstQueueFamily);
		});
}

void VK_NAMESPACE::Image::AcquireImage(uint32_t dstQueueFamily) const
{
	uint32_t& Owner = mChunk->ImageHandles.Config.ResourceOwner;
	auto OwnerCaps = mWorkingClass->GetFamilyCapabilities(dstQueueFamily);

	Core::ImageLayoutTransitionInfo transferInfo{ mChunk->ImageHandles };

	InvokeOneTimeProcess(dstQueueFamily , [this, &OwnerCaps, Owner, 
		dstQueueFamily, &transferInfo](vk::CommandBuffer CmdBuffer)
	{
		transferInfo.CmdBuffer = CmdBuffer;
		transferInfo.NewLayout = mChunk->ImageHandles.Config.CurrLayout;

		if (transferInfo.NewLayout == vk::ImageLayout::eUndefined ||
			transferInfo.NewLayout == vk::ImageLayout::ePreinitialized)
		{
			transferInfo.NewLayout = vk::ImageLayout::eGeneral;
		}

		transferInfo.SrcStageFlags = vk::PipelineStageFlagBits::eTransfer;
		transferInfo.DstStageFlags = mChunk->ImageHandles.Config.PrevStage;

		Core::Utils::FillImageAcquireMasks(transferInfo, OwnerCaps,
			transferInfo.NewLayout, transferInfo.DstStageFlags);

		transferInfo.Subresource = GetSubresourceRanges().front();

		Core::Utils::RecordImageLayoutTransition(transferInfo, Owner, dstQueueFamily);
	});

	Owner = dstQueueFamily;

	mChunk->ImageHandles.Config.CurrLayout = transferInfo.NewLayout;
	mChunk->ImageHandles.Config.PrevStage = transferInfo.DstStageFlags;
}

VK_NAMESPACE::ImageBlitInfo VK_NAMESPACE::Image::CheckBounds(const ImageBlitInfo& imageBlitInfo, const glm::uvec2& srcSize) const
{
	ImageBlitInfo blitInfo = imageBlitInfo;

	if (blitInfo.SrcEndRegion[0] == std::numeric_limits<uint32_t>::max())
		blitInfo.SrcEndRegion[0] = srcSize[0];

	if (blitInfo.SrcEndRegion[1] == std::numeric_limits<uint32_t>::max())
		blitInfo.SrcEndRegion[1] = srcSize[1];

	if (blitInfo.DstEndRegion[0] == std::numeric_limits<uint32_t>::max())
		blitInfo.DstEndRegion[0] = GetSize()[0];

	if (blitInfo.DstEndRegion[1] == std::numeric_limits<uint32_t>::max())
		blitInfo.DstEndRegion[1] = GetSize()[1];

	return blitInfo;
}

void VK_NAMESPACE::Image::MakeHollow()
{
	mChunk.Reset();
}

void VK_NAMESPACE::RecordBlitImages(vk::CommandBuffer commandBuffer,
	Image& Dst, vk::ImageLayout dstLayout, 
	const Image& Src, vk::ImageLayout srcLayout, ImageBlitInfo blitInfo)
{
	blitInfo = Dst.CheckBounds(blitInfo, Src.GetSize());

	vk::ImageBlit imageBlitInfo{};

	imageBlitInfo.srcOffsets[0] = vk::Offset3D(blitInfo.SrcBeginRegion.x, blitInfo.SrcBeginRegion.y, 0);
	imageBlitInfo.srcOffsets[1] = vk::Offset3D(blitInfo.SrcEndRegion.x, blitInfo.SrcEndRegion.y, 1);

	imageBlitInfo.dstOffsets[0] = vk::Offset3D(blitInfo.DstBeginRegion.x, blitInfo.DstBeginRegion.y, 0);
	imageBlitInfo.dstOffsets[1] = vk::Offset3D(blitInfo.DstEndRegion.x, blitInfo.DstEndRegion.y, 1);

	imageBlitInfo.srcSubresource = Src.GetSubresourceLayers().front();
	imageBlitInfo.dstSubresource = Dst.GetSubresourceLayers().front();

	commandBuffer.blitImage(Src.mChunk->ImageHandles.Handle, srcLayout,
		Dst.mChunk->ImageHandles.Handle, dstLayout, imageBlitInfo, blitInfo.Filter);
}
