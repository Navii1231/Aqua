#include "Core/vkpch.h"
#include "Device/Context.h"
#include "Memory/Framebuffer.h"
#include "Memory/RenderTargetContext.h"

template <>
VK_NAMESPACE::Framebuffer VK_NAMESPACE::Clone<VK_NAMESPACE::Framebuffer>(Context ctx, const Framebuffer& framebuffer)
{
	auto renderTargetContext = ctx.FetchRenderContextBuilder(framebuffer.GetParentContext().GetBindPoint()).Build(framebuffer.GetParentContext().GetInfo().CreateInfo);

	std::vector<ImageView> views = framebuffer.GetColorAttachments();
	AttachmentTypeFlags flags = framebuffer.GetAttachmentFlags();
	AttachmentTypeFlags depthStencilFlags = vkLib::AttachmentTypeFlagBits::eDepth;
	depthStencilFlags |= vkLib::AttachmentTypeFlagBits::eStencil;

	if (flags & depthStencilFlags)
		views.push_back(framebuffer.GetDepthStencilAttachment());

	std::vector<ImageView> viewClones{};
	viewClones.reserve(views.size());

	for (const auto& view : views)
		viewClones.emplace_back(Clone(ctx, view));

	return *renderTargetContext.ConstructFramebuffer(viewClones);
}

void VK_NAMESPACE::Framebuffer::TransitionColorAttachmentLayouts(vk::ImageLayout newLayout, vk::PipelineStageFlags newStage) const
{
	if (mInfo->ColorAttachments.empty())
		return;

	uint32_t GraphicsOwner = mWorkingClass->FindOptimalQueueFamilyIndex(vk::QueueFlagBits::eGraphics);

	InvokeOneTimeProcess(GraphicsOwner, [this, newLayout, newStage](vk::CommandBuffer commandBuffer)
		{
			RecordTransitionColorAttachmentLayoutsInternal(commandBuffer, newLayout, newStage);
			ResetColorTransitionLayoutStatesInternal();
		}
	);
}

void VK_NAMESPACE::Framebuffer::TransitionDepthAttachmentLayouts(vk::ImageLayout newLayout, vk::PipelineStageFlags newStage) const
{
	if (!mInfo->DepthStencilAttachment)
		return;

	uint32_t GraphicsOwner = mWorkingClass->FindOptimalQueueFamilyIndex(vk::QueueFlagBits::eGraphics);

	InvokeOneTimeProcess(GraphicsOwner, [this, newLayout, newStage](vk::CommandBuffer commandBuffer)
		{
			RecordTransitionDepthAttachmentLayoutsInternal(commandBuffer, newLayout, newStage);
			ResetDepthStencilTransitionLayoutStatesInternal();
		});
}

void VK_NAMESPACE::Framebuffer::BeginCommands(vk::CommandBuffer commandBuffer) const
{
	TraverseAllAttachments([commandBuffer](const Image& image, size_t index, AttachmentTypeFlags flags)
	{
		image.BeginCommands(commandBuffer);
	});
}

void VK_NAMESPACE::Framebuffer::RecordTransitionColorAttachmentLayouts(
	vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const
{
	TraverseAllColorAttachments([newLayout, newStages](const Image& image, size_t index)
	{
		image.RecordTransitionLayout(newLayout, newStages);
	});
}

void VK_NAMESPACE::Framebuffer::RecordTransitionDepthStencilAttachmentLayouts(
	vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const
{
	TraverseAllDepthStencilAttachments([newLayout, newStages](const Image& image, AttachmentTypeFlags flags)
	{
		image.RecordTransitionLayout(newLayout, newStages);
	});
}

void VK_NAMESPACE::Framebuffer::RecordBlit(const Framebuffer& src,
	const BlitInfo& blitInfo, bool retrievePreviousLayout /*= true*/) const
{
	std::vector<ImageView>& DstImages = mInfo->ColorAttachments;
	std::vector<ImageView>& SrcImages = src.mInfo->ColorAttachments;

	if (blitInfo.Flags & AttachmentTypeFlagBits::eColor)
	{
		for (size_t i = 0; i < DstImages.size() && i < SrcImages.size(); i++)
		{
			DstImages[i].GetImage().RecordBlit(SrcImages[i].GetImage(), blitInfo.BlitInfo, retrievePreviousLayout);
		}
	}

	if ((blitInfo.Flags & AttachmentTypeFlags(AttachmentTypeFlagBits::eDepth |
		AttachmentTypeFlagBits::eStencil)) && mInfo->DepthStencilAttachment &&
		src.mInfo->DepthStencilAttachment)
	{
		mInfo->DepthStencilAttachment.GetImage().RecordBlit(src.mInfo->DepthStencilAttachment.GetImage(),
			blitInfo.BlitInfo, retrievePreviousLayout);
	}
}

void VK_NAMESPACE::Framebuffer::EndCommands() const
{
	TraverseAllAttachments([]
	(const Image& image, size_t index, AttachmentTypeFlags flags)
	{
		image.EndCommands();
	});
}

void VK_NAMESPACE::Framebuffer::ResetColorTransitionLayoutStatesInternal() const
{
	TraverseAllColorAttachments([this](const Image& image, size_t index)
	{
		ImageLayoutInfo info = image.mChunk->ImageHandles.RecordedLayout;

		image.mChunk->ImageHandles.Config.CurrLayout = info.Layout;
		image.mChunk->ImageHandles.Config.PrevStage = info.Stages;
	});
}

void VK_NAMESPACE::Framebuffer::ResetDepthStencilTransitionLayoutStatesInternal() const
{
	TraverseAllDepthStencilAttachments([this](const Image& image, AttachmentTypeFlags)
	{
		ImageLayoutInfo info = image.mChunk->ImageHandles.RecordedLayout;

		image.mChunk->ImageHandles.Config.CurrLayout = info.Layout;
		image.mChunk->ImageHandles.Config.PrevStage = info.Stages;
	});
}

void VK_NAMESPACE::Framebuffer::RecordTransitionImageLayoutInternal(vk::CommandBuffer commandBuffer,
	const Image& image, vk::QueueFlags flags, vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const
{
	ImageLayoutInfo& info = image.mChunk->ImageHandles.RecordedLayout;

	Core::ImageLayoutTransitionInfo transitionInfo{ image.mChunk->ImageHandles };
	transitionInfo.CmdBuffer = commandBuffer;
	transitionInfo.OldLayout = info.Layout;
	transitionInfo.NewLayout = newLayout;
	transitionInfo.Subresource = image.GetSubresourceRanges().front();
	transitionInfo.SrcStageFlags = info.Stages;
	transitionInfo.DstStageFlags = newStages;

	Core::Utils::FillImageTransitionLayoutMasks(transitionInfo, flags,
		info.Layout, newLayout, info.Stages, newStages);

	Core::Utils::RecordImageLayoutTransition(transitionInfo);

	info = { newLayout, newStages };
}

void VK_NAMESPACE::Framebuffer::RecordTransitionColorAttachmentLayoutsInternal(vk::CommandBuffer commandBuffer,
	vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const
{
	TraverseAllColorAttachments([this, commandBuffer, newLayout, newStages]
	(const Image& image, size_t index)
	{
		auto flags = mWorkingClass->GetFamilyCapabilities(
		mWorkingClass->FindOptimalQueueFamilyIndex(vk::QueueFlagBits::eGraphics));

		RecordTransitionImageLayoutInternal(commandBuffer, image, flags, newLayout, newStages);
	});
}

void VK_NAMESPACE::Framebuffer::RecordTransitionDepthAttachmentLayoutsInternal(vk::CommandBuffer commandBuffer, vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const
{
	TraverseAllDepthStencilAttachments([this, commandBuffer, newLayout, newStages]
	(const Image& image, AttachmentTypeFlags type)
	{
		auto flags = mWorkingClass->GetFamilyCapabilities(
			mWorkingClass->FindOptimalQueueFamilyIndex(vk::QueueFlagBits::eGraphics));

		RecordTransitionImageLayoutInternal(commandBuffer, image, flags, newLayout, newStages);
	});
}
