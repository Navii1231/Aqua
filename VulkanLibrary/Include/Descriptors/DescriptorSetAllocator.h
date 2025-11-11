#pragma once
#include "DescriptorsConfig.h"
#include "DescriptorPoolBuilder.h"

VK_BEGIN

class DescriptorPoolManager;

VK_CORE_BEGIN

// Warning: not thread safe
class DescriptorSetAllocator
{
public:
	DescriptorSetAllocator() = default;

	// TODO: Check with an assert that set and binding index pairs do not repeat
	VKLIB_API Core::Ref<DescriptorResource> Allocate(const std::vector<vk::DescriptorSetLayoutBinding>& bindings);

	VKLIB_API void Reserve(size_t NewSize);

	explicit operator bool() const { return static_cast<bool>(mData); }

private:
	Core::Ref<Core::DescriptorSetAllocatorData> mData;
	Core::DescriptorPoolBuilder mBuilder;

	DescriptorSetAllocator(Core::Ref<Core::DescriptorSetAllocatorData> data, Core::DescriptorPoolBuilder builder)
		: mData(data), mBuilder(builder) {}
	
	friend class ::VK_NAMESPACE::DescriptorPoolManager;

private:
	// Helper methods...
	void ScalePoolBufferCapacity(size_t NewCap);
	vk::DescriptorPool GetEmptyDescriptorPool();
};

VK_CORE_END
VK_END
