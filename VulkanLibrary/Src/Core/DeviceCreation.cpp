#include "Core/vkpch.h"
#include "Core/Utils/DeviceCreation.h"

vk::Device VK_NAMESPACE::VK_CORE::VK_UTILS::CreateDevice(const ContextCreateInfo& createInfo)
{
	// Find all the queues which support the input queue flags
	auto [FoundFlags, QueueFamilyIndices] =
		createInfo.PhysicalDevice.GetQueueFamilyIndices(createInfo.DeviceCapabilities);

	if (FoundFlags != createInfo.DeviceCapabilities)
		throw UnsupportedQueue(FoundFlags & ~(createInfo.DeviceCapabilities));

	std::vector<vk::DeviceQueueCreateInfo> infos;

	for (auto familyIndex : QueueFamilyIndices)
	{
		vk::DeviceQueueCreateInfo queueInfo{};

		queueInfo.queueFamilyIndex = familyIndex;
		queueInfo.queueCount = std::min(
			createInfo.PhysicalDevice.QueueProps[familyIndex].queueCount, createInfo.MaxQueueCount);

		float* queuePriorities = new float[queueInfo.queueCount];

		for (uint32_t i = 0; i < queueInfo.queueCount; i++)
			queuePriorities[i] = 1.0f;

		queueInfo.pQueuePriorities = queuePriorities;

		infos.emplace_back(queueInfo);
	}

	auto AllDeviceExtensions = createInfo.PhysicalDevice.Handle.enumerateDeviceExtensionProperties();
	auto AllDeviceLayers = createInfo.PhysicalDevice.Handle.enumerateDeviceLayerProperties();

	auto UnsupportedExtensions = GetCompliment(AllDeviceExtensions, createInfo.Extensions,
		[](const vk::ExtensionProperties& props) { return props.extensionName; });

	auto UnsupportedLayers = GetCompliment(AllDeviceLayers, createInfo.Layers,
		[](const vk::LayerProperties& props) { return props.layerName;  });

	if (!UnsupportedLayers.empty() || !UnsupportedExtensions.empty())
		throw UnsupportedLayersAndExtensions(UnsupportedExtensions, UnsupportedLayers);

	vk::DeviceCreateInfo RawCreateInfo{};

	RawCreateInfo.pEnabledFeatures = &createInfo.RequiredFeatures;

	RawCreateInfo.pQueueCreateInfos = infos.data();
	RawCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(infos.size());

	RawCreateInfo.ppEnabledExtensionNames = createInfo.Extensions.data();
	RawCreateInfo.enabledExtensionCount = static_cast<uint32_t>(createInfo.Extensions.size());

	RawCreateInfo.ppEnabledLayerNames = createInfo.Layers.data();
	RawCreateInfo.enabledLayerCount = static_cast<uint32_t>(createInfo.Layers.size());

	// TODO: Do some error checking here...

	vk::Device device = createInfo.PhysicalDevice.Handle.createDevice(RawCreateInfo);

	for (auto& queueInfo : infos)
		delete[] queueInfo.pQueuePriorities;

	return device;
}

vk::Fence VK_NAMESPACE::VK_CORE::VK_UTILS::CreateFence(vk::Device device, bool Signaled)
{
	vk::FenceCreateInfo info{};

	if (Signaled)
		info.setFlags(vk::FenceCreateFlagBits::eSignaled);

	return device.createFence(info);
}

vk::Semaphore VK_NAMESPACE::VK_CORE::VK_UTILS::CreateSemaphore(vk::Device device)
{
	return device.createSemaphore({});
}

vk::Event VK_NAMESPACE::VK_CORE::VK_UTILS::CreateEvent(vk::Device device)
{
	vk::EventCreateInfo info{};
	info.setFlags(vk::EventCreateFlagBits::eDeviceOnly);

	return device.createEvent(info);
}
