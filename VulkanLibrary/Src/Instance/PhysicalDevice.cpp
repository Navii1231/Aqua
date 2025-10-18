#include "Core/vkpch.h"
#include "Device/PhysicalDevice.h"

VK_BEGIN

std::set<vk::QueueFlagBits> GetUniqueFlagBits()
{
	std::set<vk::QueueFlagBits> UniqueFlagBits;

	constexpr int gTotalFlags = 8;

	for (int i = 0; i < gTotalFlags; i++)
	{
		UniqueFlagBits.insert(vk::QueueFlagBits(1 << i));
	}

	return UniqueFlagBits;
}

VK_END

std::pair<vk::QueueFlags, VK_NAMESPACE::VK_CORE::QueueFamilyIndices>
VK_NAMESPACE::PhysicalDevice::GetQueueFamilyIndices(vk::QueueFlags flags) const
{
	std::set<uint32_t> Result;
	vk::QueueFlags FoundFlags;

	std::set<vk::QueueFlagBits> UniqueFlagBits = GetUniqueFlagBits();

	for (size_t i = 0; i < QueueProps.size(); i++)
	{
		for (auto uniqueFlag : UniqueFlagBits)
		{
			vk::QueueFlags Flag = (flags & uniqueFlag);

			if (QueueProps[i].queueFlags & Flag)
			{
				Result.insert(static_cast<uint32_t>(i));
				FoundFlags |= uniqueFlag;
			}
		}
	}

	return { FoundFlags, Result };
}

VK_NAMESPACE::VK_CORE::QueueIndexMap VK_NAMESPACE::PhysicalDevice::GetQueueIndexMap(vk::QueueFlags flags) const
{
	std::set<vk::QueueFlagBits> UniqueFlagBits = GetUniqueFlagBits();

	VK_CORE::QueueIndexMap IndexMap;

	for (size_t i = 0; i < QueueProps.size(); i++)
	{
		for (auto uniqueFlag : UniqueFlagBits)
		{
			vk::QueueFlags Flag = (flags & uniqueFlag);

			if (QueueProps[i].queueFlags & Flag)
			{
				IndexMap[uniqueFlag].insert(static_cast<uint32_t>(i));
			}
		}
	}

	return IndexMap;
}
