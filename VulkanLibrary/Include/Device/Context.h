#pragma once
#include "../Instance/PhysicalDeviceMenagerie.h"
#include "../Core/Ref.h"

#include "Descriptors/DescriptorPoolManager.h"

#include "../Process/WorkerQueue.h"
#include "../Process/WorkingClass.h"

#include "ContextConfig.h"
#include "Swapchain.h"

#include "../Memory/ResourcePool.h"

#include "../Memory/RenderContextBuilder.h"
#include "../Pipeline/PipelineBuilder.h"

VK_BEGIN

class CommandBufferAllocator;

class Context
{
public:
	Context() = default;
	VKLIB_API explicit Context(const ContextCreateInfo& info);

	// Sync stuff...
	VKLIB_API Core::Ref<vk::Semaphore> CreateSemaphore() const;
	VKLIB_API Core::Ref<vk::Fence> CreateFence(bool Signaled = true) const;
	VKLIB_API Core::Ref<vk::Event> CreateEvent() const;

	VKLIB_API void ResetFence(vk::ArrayProxy<vk::Fence> Fence);
	VKLIB_API void ResetEvent(vk::Event Event);
	VKLIB_API vk::Result WaitForFences(vk::ArrayProxy<vk::Fence> fence, bool waitforAll = true, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

	template <typename It> 
	vk::Result WaitForFences(It Begin, It End, bool waitForAll = true, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

	// Commands...
	VKLIB_API CommandPools CreateCommandPools(bool IsTransient = false, bool IsProtected = false) const;

	// Pipelines and RenderTargets...
	VKLIB_API PipelineBuilder MakePipelineBuilder() const;

	// Vulkan RenderPass wrapped in VK_NAMESPACE::RenderContext
	VKLIB_API RenderContextBuilder FetchRenderContextBuilder(vk::PipelineBindPoint bindPoint);

	// Descriptors...
	VKLIB_API DescriptorPoolManager FetchDescriptorPoolManager() const;

	// Resources and memory...
	VKLIB_API ResourcePool CreateResourcePool() const;

	// Mirrored from VK_NAMESPACE::WorkingClass for convenience
	// Worker submits tasks into queues asynchronously
	Core::Worker FetchWorker(uint32_t familyIndex) const
	{ return GetWorkingClass()->FetchWorker(familyIndex); }

	uint32_t FindOptimalQueueFamilyIndex(vk::QueueFlagBits flag) const 
	{ GetWorkingClass()->FindOptimalQueueFamilyIndex(flag); }

	vk::QueueFlags GetFamilyCapabilities(uint32_t index) const
	{ return GetWorkingClass()->GetFamilyCapabilities(index); }

	uint32_t GetQueueCount(uint32_t familyIndex) const
	{ return GetWorkingClass()->GetWorkerCount(familyIndex); }

	Core::QueueFamilyIndices GetPresentQueueFamilyIndices() const
	{ return GetWorkingClass()->GetPresentQueueFamilyIndices(); }

	// Getters...
	WorkingClassRef GetWorkingClass() const { return mWorkingClass; }

	Core::Ref<vk::Device> GetHandle() const { return mHandle; }
	const ContextCreateInfo& GetDeviceInfo() const { return *mDeviceInfo; }

	std::shared_ptr<Swapchain> GetSwapchain() const { return mSwapchain; }

	// Swapchain creation and invalidation
	VKLIB_API void InvalidateSwapchain(const SwapchainInvalidateInfo& newInfo);
	VKLIB_API void CreateSwapchain(const SwapchainInfo& info);

	void WaitIdle() const { mWorkingClass->WaitIdle(); }

	explicit operator bool() const { return static_cast<bool>(mHandle); }

	virtual ~Context() {}

private:
	Core::Ref<vk::Device> mHandle;

	Core::DescriptorPoolBuilder mDescPoolBuilder;	

	std::shared_ptr<ContextCreateInfo> mDeviceInfo;
	std::shared_ptr<WorkingClass> mWorkingClass;
	// Swapchain Stuff
	std::shared_ptr<Swapchain> mSwapchain;

private:
	// Helper functions...
	void DoSanityChecks();

	template <typename _Rsc>
	friend _Rsc Clone(Context, const _Rsc&);
};

// re-creating the resource under new context
template <typename _Rsc>
_Rsc Clone(Context ctx, const _Rsc& rsc)
{
	static_assert(!std::is_same<_Rsc, Context>::value, "can't copy the context - call 'Context Clone(Context)'");

	// by default, we call copy constructor
	return _Rsc(rsc);
}

template <typename T>
Buffer<T> Clone(Context ctx, const Buffer<T>& rsc)
{
	Buffer<T> clone = ctx.CreateResourcePool().CreateBuffer<T>(rsc.GetBufferConfig().Usage, rsc.GetBufferConfig().MemProps);

	if (ctx.GetHandle() != rsc.GetBufferRsc().Device)
	{
		// if the physical devices are different, you've to stage the buffers

		// todo: a bit shady - fails if the CPU memory is insufficient
		// throwing a memory check would be nice

		// avoiding boolean specialization
		if constexpr (std::is_same<T, bool>::value)
		{
			std::vector<uint8_t> content;
			rsc >> content;
			clone << content;

			return clone;
		}
		else
		{
			std::vector<T> content;
			rsc >> content;
			clone << content;

			return clone;
		}
	}

	CopyBuffer(clone, rsc);

	return clone;
}

//Context Clone(Context prevCtx)
//{
//	return Context(prevCtx.GetDeviceInfo());
//}

template <typename It>
vk::Result VK_NAMESPACE::Context::WaitForFences(It Begin, It End, bool waitAll, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	std::vector<vk::Fence> fences;
	fences.reserve(End - Begin);

	for (; Begin < End; Begin++)
		fences.push_back(**Begin);

	return mHandle->waitForFences(fences, waitAll, timeOut.count());
}

VK_END
