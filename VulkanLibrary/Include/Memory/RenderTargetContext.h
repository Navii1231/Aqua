#pragma once
#include "MemoryConfig.h"
#include "ImageView.h"

#include "../Core/Utils/FramebufferUtils.h"

VK_BEGIN

class Framebuffer;

enum class FramebufferConstructError
{
	eNoImage                      = 0,
	eInconsistentImageSize        = 1,
};

class RenderTargetContext
{
public:
	RenderTargetContext() = default;

	VKLIB_API Framebuffer CreateFramebuffer(uint32_t width, uint32_t height) const;
	VKLIB_API std::expected<Framebuffer, FramebufferConstructError> ConstructFramebuffer(const std::vector<ImageView>& imageViews) const;

	vk::RenderPass GetNativeHandle() const { return mContextInfo->RenderPass; }
	const RenderContextInfo& GetInfo() const { return *mContextInfo; }

	uint32_t GetColorAttachmentCount() const
	{ return static_cast<uint32_t>(mContextInfo->CreateInfo.Attachments.size() - UsingDepthOrStencilAttachment()); }

	uint32_t GetAttachmentCount() const { return static_cast<uint32_t>(mContextInfo->CreateInfo.Attachments.size()); }

	const std::vector<vkLib::ImageAttachmentInfo>& GetAttachmentInfos() const
	{ return mContextInfo->CreateInfo.Attachments; }

	const vkLib::ImageAttachmentInfo& GetDepthStencilAttachmentInfos() const
	{ return mContextInfo->CreateInfo.Attachments.back(); }

	AttachmentTypeFlags GetAttachmentFlags() const { return mAttachmentFlags; }

	bool UsingDepthOrStencilAttachment() const
	{ return mContextInfo->CreateInfo.UsingDepthAttachment || mContextInfo->CreateInfo.UsingStencilAttachment; }

	WorkingClassRef GetWorkingClass() const { return mWorkingClass; }
	vk::PipelineBindPoint GetBindPoint() const { return vk::PipelineBindPoint::eGraphics; }

	explicit operator bool() const { return static_cast<bool>(mContextInfo); }

private:
	Core::Ref<RenderContextInfo> mContextInfo;
	Core::Ref<vk::Device> mDevice;

	CommandPools mCommandPools;
	WorkingClassRef mWorkingClass;

	vk::PhysicalDevice mPhysicalDevice;

	AttachmentTypeFlags mAttachmentFlags;

	friend class RenderContextBuilder;
	friend class Swapchain;

private:
	//Helper methods
	void FillInMissingImageViews(std::vector<ImageView>& views, const glm::uvec2& size) const;

private:
	bool DepthOrStencilExist() const
	{ return static_cast<bool>(mAttachmentFlags & 
		(AttachmentTypeFlags)(AttachmentTypeFlagBits::eDepth | AttachmentTypeFlagBits::eStencil)); }

	Core::Ref<Core::ImageResource> CreateImageChunk(Core::ImageConfig& config) const;
	std::expected<glm::uvec2, FramebufferConstructError> CheckConsistentSize(const std::vector<ImageView>& imageViews) const;
};

VK_END
