#pragma once
#include "../Core/Config.h"
#include "../Instance/UnsupportedLayerAndExtensionException.h"
#include "../Process/WorkerQueue.h"

VK_BEGIN

struct SwapchainInfo
{
	uint32_t Width = 0, Height = 0;
	vk::PresentModeKHR PresentMode = vk::PresentModeKHR::eMailbox;
	vk::SurfaceFormatKHR SurfaceFormat = {vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
	Core::Ref<vk::SurfaceKHR> Surface;
};

struct SwapchainInvalidateInfo
{
	uint32_t Width = 0, Height = 0;
	vk::PresentModeKHR PresentMode = vk::PresentModeKHR::eMailbox;
};

struct SwapchainSupportDetails
{
	vk::SurfaceCapabilitiesKHR Capabilities;
	std::vector<vk::SurfaceFormatKHR> Formats;
	std::vector<vk::PresentModeKHR> PresentModes;
};

struct PhysicalDevice
{
	vk::PhysicalDevice Handle;
	vk::PhysicalDeviceProperties Props;
	vk::PhysicalDeviceFeatures Features;
	std::vector<vk::QueueFamilyProperties> QueueProps;

	Core::Ref<vk::Instance> ParentInstance;

	std::pair<vk::QueueFlags, Core::QueueFamilyIndices> GetQueueFamilyIndices(vk::QueueFlags flags) const;
	Core::QueueIndexMap GetQueueIndexMap(vk::QueueFlags flags) const;
};

struct ContextCreateInfo
{
	PhysicalDevice PhysicalDevice;
	vk::PhysicalDeviceFeatures RequiredFeatures;

	vk::QueueFlags DeviceCapabilities;
	uint32_t MaxQueueCount = 4;

	std::vector<const char*> Extensions;
	std::vector<const char*> Layers;
};

VK_END
