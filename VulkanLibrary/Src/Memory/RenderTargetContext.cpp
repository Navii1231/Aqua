#include "Core/vkpch.h"
#include "Memory/RenderTargetContext.h"
#include "Memory/Framebuffer.h"
#include "Memory/ResourcePool.h"
#include "Core/Logger.h"

VK_NAMESPACE::Framebuffer VK_NAMESPACE::RenderTargetContext::CreateFramebuffer(uint32_t width, uint32_t height) const
{
	std::vector<ImageView> views;
	views.resize(mContextInfo->CreateInfo.Attachments.size());

	FillInMissingImageViews(views, { width, height });

	return *ConstructFramebuffer(views);
}

std::expected<VK_NAMESPACE::Framebuffer, VK_NAMESPACE::FramebufferConstructError> 
	VK_NAMESPACE::RenderTargetContext::ConstructFramebuffer(const std::vector<ImageView>& imageViews) const
{
	auto CheckSize = CheckConsistentSize(imageViews);

	if (!CheckSize)
		return std::unexpected(CheckSize.error());

	std::vector<ImageView> views = imageViews;

	FillInMissingImageViews(views, *CheckSize);

	std::vector<vk::ImageView> imageViewHandles(views.size());

	for (size_t i = 0; i < imageViews.size(); i++)
		imageViewHandles[i] = views[i].GetNativeHandle();

	FramebufferInfo bufferInfo;
	bufferInfo.Width = CheckSize->x;
	bufferInfo.Height = CheckSize->y;
	bufferInfo.AttachmentFlags = mAttachmentFlags;
	bufferInfo.DepthStencilAttachment = DepthOrStencilExist() ? views.back() : ImageView();
	bufferInfo.ColorAttachments = views;
	bufferInfo.ParentContext = *this;

	if (DepthOrStencilExist())
		bufferInfo.ColorAttachments.pop_back();

	vk::FramebufferCreateInfo createInfo{};
	createInfo.setWidth(bufferInfo.Width);
	createInfo.setHeight(bufferInfo.Height);
	createInfo.setRenderPass(mContextInfo->RenderPass);
	createInfo.setLayers(1);
	createInfo.setAttachments(imageViewHandles);

	bufferInfo.Handle = mDevice->createFramebuffer(createInfo);
	auto Device = mDevice;

	Core::Ref<FramebufferInfo> bufferDataRef = Core::CreateRef<FramebufferInfo>(
		[Device](FramebufferInfo& data)
		{
			Device->destroyFramebuffer(data.Handle);
		}, bufferInfo);

	Framebuffer framebuffer{};
	framebuffer.mInfo = bufferDataRef;
	framebuffer.mDevice = Device;
	framebuffer.mWorkingClass = mWorkingClass;
	framebuffer.mCommandPools = mCommandPools;

	return framebuffer;
}

void VK_NAMESPACE::RenderTargetContext::FillInMissingImageViews(std::vector<ImageView>& AttachmentViews, const glm::uvec2& size) const
{
	std::vector<Image> ColorAttachments;
	Image DepthStencilAttachment;

	const auto& RenderCtxInfo = mContextInfo->CreateInfo;

	bool UsingDepthAttachment = RenderCtxInfo.UsingDepthAttachment;
	bool UsingStencilAttachment = RenderCtxInfo.UsingStencilAttachment;

	bool UsingDepthOrStencilAttachment = UsingStencilAttachment || UsingDepthAttachment;

	const auto& Attachments = RenderCtxInfo.Attachments;

	if (Attachments.size() == 0) return;

	ColorAttachments.resize(Attachments.size() - UsingDepthOrStencilAttachment);

	for (size_t i = 0; i < ColorAttachments.size(); i++)
	{
		if (AttachmentViews[i])
			continue;

		const auto& attachDesc = Attachments[i];

		Core::ImageConfig config;
		config.Extent = vk::Extent3D(size.x, size.y, 1);
		config.Format = attachDesc.Format;
		config.LogicalDevice = *mDevice;
		config.PhysicalDevice = mPhysicalDevice;

		// TODO: More robust assignment is needed here
		config.ResourceOwner = mWorkingClass->FindOptimalQueueFamilyIndex(vk::QueueFlagBits::eGraphics);

		config.Usage = attachDesc.Usage | vk::ImageUsageFlagBits::eTransferSrc |
			vk::ImageUsageFlagBits::eTransferDst;

		// Construct at place
		::new(&ColorAttachments[i]) Image(CreateImageChunk(config));
		ColorAttachments[i].mCommandPools = mCommandPools;
		ColorAttachments[i].mWorkingClass = GetWorkingClass();

		ColorAttachments[i].TransitionLayout(attachDesc.Layout,
			vk::PipelineStageFlagBits::eColorAttachmentOutput);

		AttachmentViews[i] = ColorAttachments[i].GetIdentityImageView();
	}

	if (DepthOrStencilExist() && !AttachmentViews.back())
	{
		const auto& depthAttach = Attachments.back();

		Core::ImageConfig config;
		config.Extent = vk::Extent3D(size.x, size.y, 1);
		config.Format = depthAttach.Format;
		config.LogicalDevice = *mDevice;
		config.PhysicalDevice = mPhysicalDevice;
		config.ResourceOwner = mWorkingClass->FindOptimalQueueFamilyIndex(vk::QueueFlagBits::eGraphics);

		config.Usage = depthAttach.Usage | vk::ImageUsageFlagBits::eTransferSrc |
			vk::ImageUsageFlagBits::eTransferDst;

		// Construct at place
		::new(&DepthStencilAttachment) Image(CreateImageChunk(config));
		DepthStencilAttachment.mCommandPools = mCommandPools;
		DepthStencilAttachment.mWorkingClass = GetWorkingClass();

		DepthStencilAttachment.TransitionLayout(depthAttach.Layout,
			vk::PipelineStageFlagBits::eColorAttachmentOutput);

		AttachmentViews.back() = DepthStencilAttachment.GetIdentityImageView();
	}
}

std::expected<glm::uvec2, VK_NAMESPACE::FramebufferConstructError> 
	VK_NAMESPACE::RenderTargetContext::CheckConsistentSize(const std::vector<ImageView>& imageViews) const
{
	std::expected<glm::uvec2, FramebufferConstructError> consistentSize;

	for (const auto& view : imageViews)
	{
		if (!view)
			continue;

		if (*consistentSize == glm::uvec2(0, 0))
			*consistentSize = view.GetSize();
		else if (*consistentSize != view.GetSize())
			return std::unexpected(FramebufferConstructError::eInconsistentImageSize);
	}

	if (*consistentSize == glm::uvec2(0, 0))
		return std::unexpected(FramebufferConstructError::eNoImage);

	return consistentSize;
}

VK_NAMESPACE::VK_CORE::Ref<VK_NAMESPACE::VK_CORE::ImageResource> VK_NAMESPACE::RenderTargetContext::
	CreateImageChunk(Core::ImageConfig& config) const
{
	Core::ImageResource chunk{};
	chunk.Device = mDevice;

	chunk.ImageHandles = Core::Utils::CreateImage(config);
	auto Device = mDevice;

	return Core::CreateRef<Core::ImageResource>([Device](Core::ImageResource& handles)
	{
		Device->freeMemory(handles.ImageHandles.Memory);
		Device->destroyImage(handles.ImageHandles.Handle);
		Device->destroyImageView(handles.ImageHandles.IdentityView);
	}, chunk);
}
