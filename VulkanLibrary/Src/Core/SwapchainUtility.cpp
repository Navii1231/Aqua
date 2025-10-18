#include "Core/vkpch.h"
#include "Core/Utils/SwapchainUtils.h"
#include "Core/Utils/FramebufferUtils.h"

VK_NAMESPACE::VK_CORE::SwapchainBundle VK_NAMESPACE::VK_CORE::VK_UTILS::CreateSwapchain(
	vk::Device device, const SwapchainInfo& info, const PhysicalDevice& physicalDevice, vk::SwapchainKHR old_swapchain)
{
	vk::SwapchainCreateInfoKHR CreateInfo;

	SwapchainSupportDetails support = Core::Utils::GetSwapchainSupport(*info.Surface, physicalDevice.Handle);

	vk::SurfaceFormatKHR format = Core::Utils::SelectSurfaceFormat(info, support);
	vk::PresentModeKHR presentMode = Core::Utils::SelectPresentMode(info, support);

	vk::SwapchainCreateInfoKHR createInfo{};

	createInfo.minImageCount = std::min(support.Capabilities.minImageCount + 1, support.Capabilities.maxImageCount);
	createInfo.surface = *info.Surface;
	createInfo.imageExtent = vk::Extent2D(info.Width, info.Height);
	createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	createInfo.imageArrayLayers = 1;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
	createInfo.imageFormat = format.format;
	createInfo.imageColorSpace = format.colorSpace;
	createInfo.presentMode = presentMode;
	createInfo.oldSwapchain = old_swapchain;

	return { device.createSwapchainKHR(createInfo), createInfo.imageFormat,
		createInfo.minImageCount, support, createInfo.imageExtent };
}

VK_NAMESPACE::SwapchainSupportDetails VK_NAMESPACE::VK_CORE::VK_UTILS
	::GetSwapchainSupport(vk::SurfaceKHR surface, vk::PhysicalDevice device)
{
	SwapchainSupportDetails support;
	support.Capabilities = device.getSurfaceCapabilitiesKHR(surface);
	support.Formats = device.getSurfaceFormatsKHR(surface);
	support.PresentModes = device.getSurfacePresentModesKHR(surface);

	return support;
}

vk::SurfaceFormatKHR VK_NAMESPACE::VK_CORE::VK_UTILS
	::SelectSurfaceFormat(const SwapchainInfo& info, const SwapchainSupportDetails& support)
{
	for (auto format : support.Formats)
	{
		if (format.format == vk::Format::eR8G8B8A8Srgb)
			return format;
	}

	return support.Formats[0];
}

vk::PresentModeKHR VK_NAMESPACE::VK_CORE::VK_UTILS
	::SelectPresentMode(const SwapchainInfo& info, const SwapchainSupportDetails& support)
{
	for (auto presentMode : support.PresentModes)
	{
		if (presentMode == info.PresentMode)
			return presentMode;
	}

	return vk::PresentModeKHR::eFifo;
}
