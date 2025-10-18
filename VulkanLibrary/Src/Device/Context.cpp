#include "Core/vkpch.h"
#include "Device/Context.h"

#include "Process/Commands.h"

#include "Core/Utils/DeviceCreation.h"
#include "Core/Utils/FramebufferUtils.h"
#include "Core/Utils/SwapchainUtils.h"

VK_NAMESPACE::Context::Context(const ContextCreateInfo& info)
	: mHandle(), mDeviceInfo(std::make_shared<ContextCreateInfo>(info)), mSwapchain()
{
	DoSanityChecks();

	auto instance = mDeviceInfo->PhysicalDevice.ParentInstance;

	mHandle = Core::CreateRef<vk::Device>([instance](vk::Device& device) { device.destroy(); },
		Core::Utils::CreateDevice(*mDeviceInfo));

	// Retrieving queues from the device...
	auto queueCapabilities = info.PhysicalDevice.GetQueueIndexMap(info.DeviceCapabilities);
	auto [found, indices] = info.PhysicalDevice.GetQueueFamilyIndices(info.DeviceCapabilities);

	Core::QueueFamilyMap<std::vector<Core::Ref<Core::WorkerQueue>>> workers;

	auto device = mHandle;

	for (auto index : indices)
	{
		auto& familyProps = info.PhysicalDevice.QueueProps[index];
		auto& familyRefList = workers[index];

		size_t count = std::min(info.MaxQueueCount, familyProps.queueCount);

		for (size_t i = 0; i < count; i++)
		{
			auto familyRef = Core::CreateRef<Core::WorkerQueue>([device](Core::WorkerQueue& worker) { }, 
				mHandle->getQueue(index, static_cast<uint32_t>(i)), static_cast<uint32_t>(i), *mHandle);

			familyRefList.emplace_back(familyRef);
		}
	}

	// Creating a working class...
	mWorkingClass = std::shared_ptr<WorkingClass>(new WorkingClass(workers, indices, queueCapabilities, mDeviceInfo->PhysicalDevice.QueueProps, mHandle));

	mDescPoolBuilder = { mHandle };
}

VK_NAMESPACE::Core::Ref<vk::Semaphore> VK_NAMESPACE::Context::CreateSemaphore() const
{
	auto Device = mHandle;

	return Core::CreateRef<vk::Semaphore>([Device](vk::Semaphore semaphore) 
	{ Device->destroySemaphore(semaphore); }, Core::Utils::CreateSemaphore(*mHandle));
}

VK_NAMESPACE::Core::Ref<vk::Fence> VK_NAMESPACE::Context::CreateFence(bool Signaled) const
{
	auto Device = mHandle;

	return Core::CreateRef<vk::Fence>([Device](vk::Fence fence) 
	{ Device->destroyFence(fence); }, Core::Utils::CreateFence(*mHandle, Signaled));
}

VK_NAMESPACE::Core::Ref<vk::Event> VK_NAMESPACE::Context::CreateEvent() const
{
	auto Device = mHandle;

	return Core::CreateRef<vk::Event>([Device](vk::Event event_) 
	{Device->destroyEvent(event_); }, Core::Utils::CreateEvent(*mHandle));
}

void VK_NAMESPACE::Context::ResetFence(vk::ArrayProxy<vk::Fence> fences)
{
	mHandle->resetFences(fences);
}

void VK_NAMESPACE::Context::ResetEvent(vk::Event Event)
{
	mHandle->resetEvent(Event);
}

vk::Result VK_NAMESPACE::Context::WaitForFences(vk::ArrayProxy<vk::Fence> fences, 
	bool waitForAll, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	return mHandle->waitForFences(fences, waitForAll, timeOut.count());
}

VK_NAMESPACE::CommandPools VK_NAMESPACE::Context::CreateCommandPools(
	bool IsTransient /*= false*/, bool IsProtected /*= false*/) const
{
	vk::CommandPoolCreateFlags CreationFlags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

	if (IsTransient)
		CreationFlags |= vk::CommandPoolCreateFlagBits::eTransient;

	if (IsProtected)
		CreationFlags |= vk::CommandPoolCreateFlagBits::eProtected;

	return { mHandle, mWorkingClass->GetWorkerFamilyIndices(), CreationFlags, GetWorkingClass()};
}

VK_NAMESPACE::PipelineBuilder VK_NAMESPACE::Context::MakePipelineBuilder() const
{
	PipelineBuilder builder{};
	builder.mDevice = mHandle;
	builder.mResourcePool = CreateResourcePool();

	PipelineBuilderData data{};
	vk::PipelineCacheCreateInfo cacheInfo{};
	data.Cache = mHandle->createPipelineCache(cacheInfo);

	auto Device = mHandle;

	builder.mData = Core::CreateRef<PipelineBuilderData>([Device](const PipelineBuilderData& builderData)
	{
		Device->destroyPipelineCache(builderData.Cache);
	}, data);

	builder.mDevice = mHandle;
	builder.mDescPoolManager = FetchDescriptorPoolManager();

	return builder;
}

VK_NAMESPACE::DescriptorPoolManager VK_NAMESPACE::Context::FetchDescriptorPoolManager() const
{
	DescriptorPoolManager manager;
	manager.mPoolBuilder = mDescPoolBuilder;

	return manager;
}

VK_NAMESPACE::ResourcePool VK_NAMESPACE::Context::CreateResourcePool() const
{
	ResourcePool pool{};
	pool.mDevice = mHandle;
	pool.mPhysicalDevice = mDeviceInfo->PhysicalDevice;
	pool.mBufferCommandPools = CreateCommandPools(true);
	pool.mImageCommandPools = CreateCommandPools(true);
	pool.mWorkingClass = mWorkingClass;

	return pool;
}

VK_NAMESPACE::RenderContextBuilder VK_NAMESPACE::Context::FetchRenderContextBuilder(vk::PipelineBindPoint bindPoint)
{
	RenderContextBuilder Context;
	Context.mDevice = mHandle;
	Context.mPhysicalDevice = mDeviceInfo->PhysicalDevice.Handle;
	Context.mQueueManager = GetWorkingClass();
	Context.mCommandPools = CreateCommandPools(true);
	Context.mBindPoint = bindPoint;

	return Context;
}

void VK_NAMESPACE::Context::InvalidateSwapchain(const SwapchainInvalidateInfo& newInfo)
{
	SwapchainInfo swapchainInfo{};
	swapchainInfo.Width = newInfo.Width;
	swapchainInfo.Height = newInfo.Height;
	swapchainInfo.PresentMode = newInfo.PresentMode;
	swapchainInfo.Surface = mSwapchain->mInfo.Surface;

	CreateSwapchain(swapchainInfo);
}

void VK_NAMESPACE::Context::CreateSwapchain(const SwapchainInfo& info)
{
	mSwapchain = std::shared_ptr<Swapchain>(new Swapchain(mHandle, mDeviceInfo->PhysicalDevice, 
		FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics), info, 
		GetWorkingClass(), CreateCommandPools()));
}

void VK_NAMESPACE::Context::DoSanityChecks()
{
	// so far so good
	auto& extensions = mDeviceInfo->Extensions;
	auto found = std::find(extensions.begin(), extensions.end(), VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	if (found == extensions.end())
		extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}
