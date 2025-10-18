#pragma once
#include "../Config.h"
#include "../../Device/ContextConfig.h"
#include "../../Device/PhysicalDevice.h"

VK_BEGIN
VK_CORE_BEGIN

struct SwapchainBundle
{
	vk::SwapchainKHR Handle = nullptr;
	vk::Format ImageFormat = vk::Format::eA2R10G10B10UscaledPack32;
	uint32_t ImageCount = -1;
	SwapchainSupportDetails Support;
	vk::Extent2D ImageExtent{};
};

VK_UTILS_BEGIN

// Swapchain...
VKLIB_API SwapchainBundle CreateSwapchain(vk::Device device, const SwapchainInfo& info,
	const PhysicalDevice& physicalDevice, vk::SwapchainKHR old_swapchain = nullptr);

VKLIB_API SwapchainSupportDetails GetSwapchainSupport(vk::SurfaceKHR surface, vk::PhysicalDevice device);
VKLIB_API vk::SurfaceFormatKHR SelectSurfaceFormat(const SwapchainInfo& info, const SwapchainSupportDetails& support);
VKLIB_API vk::PresentModeKHR SelectPresentMode(const SwapchainInfo& info, const SwapchainSupportDetails& support);

VK_UTILS_END
VK_CORE_END
VK_END
