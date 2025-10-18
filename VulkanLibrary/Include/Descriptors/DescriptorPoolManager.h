#pragma once
#include "DescriptorsConfig.h"
#include "DescriptorSetAllocator.h"
#include "DescriptorPoolBuilder.h"

VK_BEGIN

class DescriptorPoolManager
{
public:
	DescriptorPoolManager() = default;

	VKLIB_API Core::DescriptorSetAllocator FetchAllocator(
		vk::DescriptorPoolCreateFlags createFlags, size_t batchSize = 100) const;

	VKLIB_API Core::DescriptorSetAllocator FetchAllocator(
		vk::DescriptorPoolCreateFlags createFlags, 
		const std::unordered_set<vk::DescriptorType> Types,
		size_t batchSize = 100) const;

	Core::DescriptorPoolBuilder GetBuilder() const { return mPoolBuilder; }

private:
	Core::DescriptorPoolBuilder mPoolBuilder;

	friend class Context;
};

VK_END
