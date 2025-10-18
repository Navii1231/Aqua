#include "Core/vkpch.h"
#include "Device/Swapchain.h"
#include "Core/Utils/SwapchainUtils.h"

VK_NAMESPACE::Swapchain::~Swapchain()
{
	for (const auto& frame : mData.Backbuffers)
	{
		mCommandAllocator.Free(frame.CommandBuffer);
	}
}

void VK_NAMESPACE::Swapchain::Resize(uint32_t width, uint32_t height)
{
	mInfo.Width = width;
	mInfo.Height = height;

	mWorkingClass->WaitIdle();

	if (mData.Handle)
	{
		for (const auto& frame : mData.Backbuffers)
		{
			mCommandAllocator.Free(frame.CommandBuffer);
		}
	}

	mData.Backbuffers.clear();
	mData.Semaphores.clear();

	if(width != 0 && height != 0)
		CreateSwapchain();
}

VK_NAMESPACE::Swapchain::Swapchain(Core::Ref<vk::Device> device, PhysicalDevice physicalDevice,
	const RenderContextBuilder& builder, const SwapchainInfo& info, 
	WorkingClassRef queueManager, CommandPools commandPools)
	: mDevice(device), mRenderContextBuilder(builder), 
	mInfo(info), mWorkingClass(queueManager), mCommandPools(commandPools), mPhysicalDevice(physicalDevice)
{
	AcquireRscs();
	CreateSwapchain();
}

void VK_NAMESPACE::Swapchain::InsertImageHandle(Image& image,
	vk::Image handle, vk::ImageView viewHandle, const Core::SwapchainBundle& bundle)
{
	Core::ImageResource chunk;
	chunk.Device = mDevice;

	Core::Image handles{};
	handles.Handle = handle;

	handles.Config.CurrLayout = vk::ImageLayout::eUndefined;
	handles.Config.Extent = vk::Extent3D(bundle.ImageExtent.width, bundle.ImageExtent.height, 1);
	handles.Config.Format = bundle.ImageFormat;
	handles.Config.LogicalDevice = *mDevice;
	handles.Config.PhysicalDevice = mPhysicalDevice.Handle;
	handles.Config.PrevStage = vk::PipelineStageFlagBits::eTopOfPipe;
	handles.Config.ResourceOwner = 0;
	handles.Config.Type = vk::ImageType::e2D;
	handles.Config.ViewType = vk::ImageViewType::e2D;
	handles.Config.Usage = vk::ImageUsageFlagBits::eColorAttachment;

	handles.IdentityView = viewHandle;
	handles.Memory = nullptr;
	handles.MemReq = vk::MemoryRequirements();

	auto SwapchainHandle = mData.Handle;
	chunk.ImageHandles = handles;

	::new(&image) Image();
	image.mChunk = Core::CreateRef<Core::ImageResource>(
		[SwapchainHandle](Core::ImageResource& imageChunk)
	{
		// Do nothing, resource is owned by the swapchain...
	}, chunk);

	image.mWorkingClass = GetWorkingClass();
	image.mCommandPools = mCommandPools;
}

void VK_NAMESPACE::Swapchain::AcquireRscs()
{
	const auto& QueueCapabilites = mWorkingClass->GetFamilyIndicesByCapabilityMap();

	mGFXFamilyIdx = *QueueCapabilites.at(vk::QueueFlagBits::eGraphics).begin();

	SwapchainSupportDetails support = Core::Utils::GetSwapchainSupport(*mInfo.Surface, mPhysicalDevice.Handle);

	vk::SurfaceFormatKHR format = Core::Utils::SelectSurfaceFormat(mInfo, support);
	vk::PresentModeKHR presentMode = Core::Utils::SelectPresentMode(mInfo, support);

	mInfo.PresentMode = presentMode;
	mInfo.SurfaceFormat = format;

	RenderContextCreateInfo contextInfo{};
	contextInfo.Attachments.resize(1);
	contextInfo.Attachments[0].Format = format.format;
	contextInfo.Attachments[0].Usage = vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eTransferDst;

	contextInfo.Attachments[0].Layout = vk::ImageLayout::eColorAttachmentOptimal;

	mRenderCtx = mRenderContextBuilder.Build(contextInfo);
	mCommandAllocator = mCommandPools[mGFXFamilyIdx];
}

void VK_NAMESPACE::Swapchain::CreateSwapchain()
{	
	auto Device = mDevice;

	auto Bundle = Core::Utils::CreateSwapchain(*mDevice, mInfo, 
		mPhysicalDevice, mData.Handle ? *mData.Handle : nullptr);

	mData.Handle = Core::CreateRef<vk::SwapchainKHR>([Device](vk::SwapchainKHR& swapchain)
	{ Device->destroySwapchainKHR(swapchain); }, Bundle.Handle);

	// Creating ImageViews and Framebuffers here...
	uint32_t FrameCount = Bundle.ImageCount;

	auto RenderPass = mRenderCtx.GetInfo().RenderPass;

	auto Images = Device->getSwapchainImagesKHR(*mData.Handle);

	auto& Frames = mData.Backbuffers;
	auto& Semaphores = mData.Semaphores;

	Frames.clear();
	Frames.resize(FrameCount);
	Semaphores.clear();
	Semaphores.resize(FrameCount);

	auto SwapchainHandle = mData.Handle;

	for (uint32_t i = 0; i < FrameCount; i++)
	{
		ImageViewCreateInfo viewInfo{};
		viewInfo.Type = vk::ImageViewType::e2D;
		viewInfo.Subresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		viewInfo.Subresource.baseArrayLayer = 0;
		viewInfo.Subresource.baseMipLevel = 0;
		viewInfo.Subresource.layerCount = 1;
		viewInfo.Subresource.levelCount = 1;
		viewInfo.Format = Bundle.ImageFormat;

		vk::ImageView imageViewHandle = Core::Utils::CreateImageView(*Device, Images[i], viewInfo);

		Image currImage{};
		InsertImageHandle(currImage, Images[i], imageViewHandle, Bundle);
		currImage.mChunk->ImageHandles.IdentityViewInfo = viewInfo;

		Framebuffer renderTarget{};
		FramebufferInfo data;

		data.ColorAttachments.push_back(currImage.GetIdentityImageView());

		vk::ImageView view = data.ColorAttachments[0].GetNativeHandle();

		vk::FramebufferCreateInfo framebufferInfo{};
		framebufferInfo.setAttachments(view);
		framebufferInfo.setWidth(Bundle.ImageExtent.width);
		framebufferInfo.setHeight(Bundle.ImageExtent.height);
		framebufferInfo.setLayers(1);
		framebufferInfo.setRenderPass(RenderPass);

		data.Handle = Device->createFramebuffer(framebufferInfo);
		data.Width = Bundle.ImageExtent.width;
		data.Height = Bundle.ImageExtent.height;
		data.ParentContext = mRenderCtx;

		renderTarget.mInfo = Core::CreateRef<FramebufferInfo>([Device](FramebufferInfo& data)
		{
			Device->destroyFramebuffer(data.Handle);
			Device->destroyImageView(data.ColorAttachments[0].GetNativeHandle());
		}, data);

		renderTarget.mDevice = Device;
		renderTarget.mWorkingClass = mWorkingClass;
		renderTarget.mCommandPools = mCommandPools;

		renderTarget.TransitionColorAttachmentLayouts(vk::ImageLayout::ePresentSrcKHR);

		BackbufferFrame Frame;
		FrameSemaphores semaphores;

		semaphores.RenderCompleteSemaphore = Core::CreateRef<vk::Semaphore>([Device](vk::Semaphore& semaphore)
		{ Device->destroySemaphore(semaphore); }, mDevice->createSemaphore({}));

		semaphores.ImageAcquiredSemaphore = Core::CreateRef<vk::Semaphore>([Device](vk::Semaphore& semaphore)
		{ Device->destroySemaphore(semaphore); }, Device->createSemaphore({}));

		Frame.BackbufferView = renderTarget.GetColorAttachments().front();
		Frame.Backbuffer = *Frame.BackbufferView;
		Frame.CommandBuffer = mCommandAllocator.Allocate();
		Frame.CommandPool = mCommandPools;
		Frame.Framebuffer = renderTarget;

		Frame.Fence = Core::CreateRef<vk::Fence>([Device](vk::Fence& fence)
		{
			Device->destroyFence(fence);
		}, Device->createFence({ vk::FenceCreateFlagBits::eSignaled }));

		Frames[i] = Frame;
		Semaphores[i] = semaphores;
	}
}
