#pragma once
#include "InstanceMenagerie.h"
#include "../Device/PhysicalDevice.h"

VK_BEGIN

inline uint64_t CalcDeviceScoreDefault(const PhysicalDevice& device)
{
	uint64_t score = 0;

	switch (device.Props.deviceType)
	{
		case VULKAN_HPP_NAMESPACE::PhysicalDeviceType::eOther:
			break;
		case VULKAN_HPP_NAMESPACE::PhysicalDeviceType::eVirtualGpu:
			score = 1;
			break;
		case VULKAN_HPP_NAMESPACE::PhysicalDeviceType::eCpu:
			score = 2;
			break;
		case VULKAN_HPP_NAMESPACE::PhysicalDeviceType::eIntegratedGpu:
			score = 3;
			break;
		case VULKAN_HPP_NAMESPACE::PhysicalDeviceType::eDiscreteGpu:
			score = 4;
			break;
		default:
			break;
	}

	score <<= 32;

	score += device.Props.limits.maxImageDimension2D;

	return score;
}

class PhysicalDeviceMenagerie
{
public:
	explicit PhysicalDeviceMenagerie(Core::Ref<vk::Instance> instance)
		: mInstance(instance)
	{
		auto devices = mInstance->enumeratePhysicalDevices();
		
		for (auto& device : devices)
		{
			auto props = device.getProperties();
			auto features = device.getFeatures();
			auto queueProps = device.getQueueFamilyProperties();

			mDeviceProperties.push_back({ device, props, features, queueProps, mInstance });
		}
	}

	template <typename Fn>
	std::vector<PhysicalDevice> SelectDevices(Fn proc) const
	{
		auto Devices = mDeviceProperties;

		std::sort(Devices.begin(), Devices.end(), [proc](const PhysicalDevice& first, const PhysicalDevice& second)
		{
			return proc(first) > proc(second);
		});

		return Devices;
	}

	PhysicalDevice& operator[](size_t index) { return mDeviceProperties[index]; }
	const PhysicalDevice& operator[](size_t index) const { return mDeviceProperties[index]; }

	auto begin() { return mDeviceProperties.begin(); }
	auto begin() const { return mDeviceProperties.begin(); }

	auto end() { return mDeviceProperties.end(); }
	auto end() const { return mDeviceProperties.end(); }

private:
	Core::Ref<vk::Instance> mInstance;

	std::vector<PhysicalDevice> mDeviceProperties;
};

VK_END
