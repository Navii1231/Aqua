#pragma once
#include "../Core/Config.h"
#include "../Core/Ref.h"

#include "PhysicalDevice.h"
#include "../Process/Commands.h"

#include "../Memory/Framebuffer.h"
#include "../Memory/RenderTargetContext.h"
#include "../Memory/RenderContextBuilder.h"

#include "../Memory/Image.h"

#include "../Core/Utils/SwapchainUtils.h"

VK_BEGIN


// mirroring the ImGui ImGui_ImplVulkanH_Frame struct to make it compatible
struct BackbufferFrame
{
	CommandPools CommandPool;
	vk::CommandBuffer CommandBuffer;
	Core::Ref<vk::Fence> Fence;
	Image Backbuffer;
	ImageView BackbufferView;
	Framebuffer Framebuffer;
};

// mirroring the ImGui_ImplVulkan_FrameSemaphores
struct FrameSemaphores
{
	Core::Ref<vk::Semaphore> ImageAcquiredSemaphore;
	Core::Ref<vk::Semaphore> RenderCompleteSemaphore;
};

struct SwapchainData
{
	Core::Ref<vk::SwapchainKHR> Handle;
	std::vector<BackbufferFrame> Backbuffers;
	std::vector<FrameSemaphores> Semaphores;

	uint32_t SemaphoreIdx = 0;
	uint32_t FrameIdx = 0;
};

class Swapchain
{
public:
	VKLIB_API ~Swapchain();

	vk::ResultValue<uint32_t> AcquireNextFrame(uint64_t timeout = UINT64_MAX) const
	{
		vk::Semaphore acquiredSemaphore = *mData.Semaphores[mData.SemaphoreIdx].ImageAcquiredSemaphore;
		return mDevice->acquireNextImageKHR(*mData.Handle, timeout, acquiredSemaphore);
	}

	void NextFrame()
	{ mData.SemaphoreIdx = (mData.SemaphoreIdx + 1) % mData.Semaphores.size(); }

	Core::Ref<vk::SwapchainKHR> GetHandle() const { return mData.Handle; }
	//Core::Ref<vk::RenderPass> GetRenderPass() const { return mRenderPass; }

	VKLIB_API void Resize(uint32_t width, uint32_t height);

	RenderTargetContext GetRenderCtx() const { return mRenderCtx; }
	FrameSemaphores GetSemaphoreFrame() const { return mData.Semaphores[mData.SemaphoreIdx]; }
	SwapchainSupportDetails GetSupportDetails() const { return mSupportDetails; }
	SwapchainInfo GetInfo() const { return mInfo; }
	SwapchainData GetSwapchainData() const { return mData; }
	WorkingClassRef GetWorkingClass() const { return mWorkingClass; }

	Core::Ref<vk::SurfaceKHR> GetSurface() const { return mInfo.Surface; }

	const BackbufferFrame& operator[](size_t index) const { return mData.Backbuffers[index]; }

private:
	RenderContextBuilder mRenderContextBuilder;
	RenderTargetContext mRenderCtx;

	SwapchainSupportDetails mSupportDetails;
	SwapchainInfo mInfo;

	WorkingClassRef mWorkingClass;
	CommandPools mCommandPools;
	CommandBufferAllocator mCommandAllocator;

	uint32_t mGFXFamilyIdx;

	SwapchainData mData;

	Core::Ref<vk::Device> mDevice;
	PhysicalDevice mPhysicalDevice;

	Swapchain(Core::Ref<vk::Device> device, PhysicalDevice physicalDevice,
		const RenderContextBuilder& builder, const SwapchainInfo& info,
		WorkingClassRef mQueueManager, CommandPools mCommandPools);

	Swapchain(const Swapchain&) = delete;
	Swapchain& operator=(const Swapchain&) = delete;

	friend class Context;

private:
	void InsertImageHandle(Image& image, vk::Image handle, vk::ImageView viewHandle, const Core::SwapchainBundle& bundle);
	void AcquireRscs();
	void CreateSwapchain();
};

VK_END

