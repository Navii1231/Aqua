#pragma once
#include "DescriptorsConfig.h"

VK_BEGIN
VK_CORE_BEGIN

class DescriptorPoolBuilder
{
public:
	DescriptorPoolBuilder() = default;

	vk::DescriptorPool Build(const Core::DescriptorSetAllocatorInfo& allocInfo) const
	{
		std::vector<vk::DescriptorPoolSize> PoolSizes;

		for (auto DescType : allocInfo.Types)
			PoolSizes.emplace_back(DescType, allocInfo.BindingCount);

		vk::DescriptorPoolCreateInfo createInfo{};
		createInfo.setFlags(allocInfo.Flags);
		createInfo.setPoolSizes(PoolSizes);
		createInfo.setMaxSets(allocInfo.BatchSize);

		return mDevice->createDescriptorPool(createInfo);
	}

	void Destroy(vk::DescriptorPool pool) const
	{ mDevice->destroyDescriptorPool(pool); }

	void SetDevice(Core::Ref<vk::Device> device) { mDevice = device; }
	Core::Ref<vk::Device> GetDevice() const { return mDevice; }

private:
	DescriptorPoolBuilder(Core::Ref<vk::Device> device)
		: mDevice(device) {}

	Core::Ref<vk::Device> mDevice;

	friend class Context;
};

VK_CORE_END
VK_END
