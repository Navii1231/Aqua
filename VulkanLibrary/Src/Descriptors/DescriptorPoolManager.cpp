#include "Core/vkpch.h"
#include "Descriptors/DescriptorPoolManager.h"
#include "Descriptors/DescriptorSetAllocator.h"

VK_NAMESPACE::VK_CORE::DescriptorSetAllocator VK_NAMESPACE::DescriptorPoolManager::
	FetchAllocator(vk::DescriptorPoolCreateFlags createFlags, size_t batchSize /*= 100*/) const
{
	// TODO: Include all necessary types...

	std::unordered_set<vk::DescriptorType> descTypes;

	descTypes.insert(vk::DescriptorType::eUniformBuffer);
	descTypes.insert(vk::DescriptorType::eStorageBuffer);
	descTypes.insert(vk::DescriptorType::eCombinedImageSampler);
	descTypes.insert(vk::DescriptorType::eStorageImage);
	descTypes.insert(vk::DescriptorType::eSampledImage);
	descTypes.insert(vk::DescriptorType::eSampler);

	return FetchAllocator(createFlags, descTypes, batchSize);
}

VK_NAMESPACE::VK_CORE::DescriptorSetAllocator VK_NAMESPACE::DescriptorPoolManager::
	FetchAllocator(vk::DescriptorPoolCreateFlags createFlags,
		const std::unordered_set<vk::DescriptorType> Types, size_t batchSize) const
{
	Core::DescriptorSetAllocatorInfo allocatorInfo{};
	allocatorInfo.Flags = createFlags;
	allocatorInfo.Types = Types;
	allocatorInfo.BatchSize = static_cast<uint32_t>(batchSize);

	Core::DescriptorSetAllocatorData AllocatorData{};
	AllocatorData.Device = mPoolBuilder.GetDevice();
	AllocatorData.Info = allocatorInfo;

	Core::Ref<Core::DescriptorSetAllocatorData> allocatorInfoRef =
		Core::CreateRef<Core::DescriptorSetAllocatorData>(Core::DescriptorSetAllocatorDataDeleter(), AllocatorData);

	return { allocatorInfoRef, mPoolBuilder };
}
