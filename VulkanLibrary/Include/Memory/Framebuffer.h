#pragma once
#include "MemoryConfig.h"
#include "Image.h"
#include "ImageView.h"

#include "RenderTargetContext.h"

#include "../Core/Utils/FramebufferUtils.h"

VK_BEGIN

class RenderTargetContext;

struct FramebufferInfo
{
	vk::Framebuffer Handle;

	RenderTargetContext ParentContext;

	std::vector<ImageView> ColorAttachments;

	// Vulkan supports only single depth/stencil attachment per framebuffer...
	ImageView DepthStencilAttachment;

	AttachmentTypeFlags AttachmentFlags = AttachmentTypeFlags();

	uint32_t Width = 0;
	uint32_t Height = 0;
};

struct BlitInfo
{
	AttachmentTypeFlags Flags = AttachmentTypeFlagBits::eColor;
	ImageBlitInfo BlitInfo;
};

class Framebuffer : public RecordableResource
{
public:
	Framebuffer() = default;

	VKLIB_API void TransitionColorAttachmentLayouts(vk::ImageLayout newLayout, vk::PipelineStageFlags newStage = vk::PipelineStageFlagBits::eTopOfPipe) const;
	VKLIB_API void TransitionDepthAttachmentLayouts(vk::ImageLayout newLayout, vk::PipelineStageFlags newStage = vk::PipelineStageFlagBits::eTopOfPipe) const;

	// Recording of resources manipulation...
	VKLIB_API virtual void BeginCommands(vk::CommandBuffer commandBuffer) const override;

	VKLIB_API void RecordTransitionColorAttachmentLayouts(vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const;
	VKLIB_API void RecordTransitionDepthStencilAttachmentLayouts(
		vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const;

	VKLIB_API void RecordBlit(const Framebuffer& src, const BlitInfo& blitInfo, bool retrievePreviousLayout = true) const;

	VKLIB_API virtual void EndCommands() const override;

	const FramebufferInfo& GetFramebufferData() const { return *mInfo; }
	vk::Framebuffer GetNativeHandle() const { return mInfo->Handle; }

	glm::uvec2 GetResolution() const { return { mInfo->Width, mInfo->Height }; }
	AttachmentTypeFlags GetAttachmentFlags() const { return AttachmentTypeFlagBits::eColor; }

	std::vector<ImageView> GetColorAttachments() const { return mInfo->ColorAttachments; }
	ImageView GetDepthStencilAttachment() const { return mInfo->DepthStencilAttachment; }

	RenderTargetContext GetParentContext() const { return mInfo->ParentContext; }

	explicit operator bool() const { return static_cast<bool>(mInfo); }

private:
	Core::Ref<FramebufferInfo> mInfo;
	Core::Ref<vk::Device> mDevice;

	// Transition internal state...

	friend class RenderTargetContext;
	friend class Swapchain;

	template <typename BasePipeline>
	friend class BasicGraphicsPipeline;

	template <typename Fn>
	void TraverseAllAttachments(Fn&& fn);

	template <typename Fn>
	void TraverseAllAttachments(Fn&& fn) const;

	template <typename Fn>
	void TraverseAllColorAttachments(Fn&& fn);

	template <typename Fn>
	void TraverseAllColorAttachments(Fn&& fn) const;

	template <typename Fn>
	void TraverseAllDepthStencilAttachments(Fn&& fn);

	template <typename Fn>
	void TraverseAllDepthStencilAttachments(Fn&& fn) const;

	void RecordTransitionImageLayoutInternal(vk::CommandBuffer commandBuffer, const Image& image, 
		vk::QueueFlags flags, vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const;

	void RecordTransitionColorAttachmentLayoutsInternal(vk::CommandBuffer commandBuffer,
		vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const;
	void RecordTransitionDepthAttachmentLayoutsInternal(vk::CommandBuffer commandBuffer,
		vk::ImageLayout newLayout, vk::PipelineStageFlags newStages) const;

	void ResetColorTransitionLayoutStatesInternal() const;
	void ResetDepthStencilTransitionLayoutStatesInternal() const;
};

template <>
Framebuffer Clone<Framebuffer>(Context ctx, const Framebuffer& framebuffer);

template <typename Fn>
void VK_NAMESPACE::Framebuffer::TraverseAllDepthStencilAttachments(Fn&& fn) const
{
	if (mInfo->DepthStencilAttachment)
		fn(*mInfo->DepthStencilAttachment,
			(AttachmentTypeFlags) (mInfo->AttachmentFlags &
				(AttachmentTypeFlags) ~(AttachmentTypeFlagBits::eColor)));
}

template <typename Fn>
void VK_NAMESPACE::Framebuffer::TraverseAllDepthStencilAttachments(Fn&& fn)
{
	if (mInfo->DepthStencilAttachment)
		fn(*mInfo->DepthStencilAttachment,
			(AttachmentTypeFlags) (mInfo->AttachmentFlags &
				(AttachmentTypeFlags) ~(AttachmentTypeFlagBits::eColor)));
}

template <typename Fn>
void VK_NAMESPACE::Framebuffer::TraverseAllColorAttachments(Fn&& fn) const
{
	std::vector<ImageView>& Images = mInfo->ColorAttachments;

	for (size_t i = 0; i < Images.size(); i++)
		fn(*Images[i], i);
}

template <typename Fn>
void VK_NAMESPACE::Framebuffer::TraverseAllColorAttachments(Fn&& fn)
{
	std::vector<ImageView>& Images = mInfo->ColorAttachments;

	for (size_t i = 0; i < Images.size(); i++)
		fn(*Images[i], i);
}

template <typename Fn>
void VK_NAMESPACE::Framebuffer::TraverseAllAttachments(Fn&& fn) const
{
	std::vector<ImageView>& Images = mInfo->ColorAttachments;

	for (size_t i = 0; i < Images.size(); i++)
		fn(*Images[i], i, AttachmentTypeFlagBits::eColor);

	if(mInfo->DepthStencilAttachment)
		fn(*mInfo->DepthStencilAttachment, 0, 
			(AttachmentTypeFlags)(mInfo->AttachmentFlags &
				(AttachmentTypeFlags)~(AttachmentTypeFlagBits::eColor)));
}

template <typename Fn>
void VK_NAMESPACE::Framebuffer::TraverseAllAttachments(Fn&& fn)
{
	std::vector<ImageView>& Images = mInfo->ColorAttachments;

	for (size_t i = 0; i < Images.size(); i++)
		fn(*Images[i], i, AttachmentTypeFlagBits::eColor);

	if (mInfo->DepthStencilAttachment)
		fn(*mInfo->DepthStencilAttachment, 0, 
			(AttachmentTypeFlags) (mInfo->AttachmentFlags & 
				(AttachmentTypeFlags)~(AttachmentTypeFlagBits::eColor)));
}

VK_END
