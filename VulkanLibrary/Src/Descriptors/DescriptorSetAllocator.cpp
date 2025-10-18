#include "Core/vkpch.h"
#include "Descriptors/DescriptorSetAllocator.h"


VK_NAMESPACE::Core::Ref<VK_NAMESPACE::DescriptorResource> 
	VK_NAMESPACE::VK_CORE::DescriptorSetAllocator::Allocate(
		const std::vector<vk::DescriptorSetLayoutBinding>& bindings)
{
	vk::DescriptorSetLayoutCreateInfo createInfo{};
	createInfo.setBindings(bindings);

	DescriptorResource Resource;
	Resource.Layout = mData->Device->createDescriptorSetLayout(createInfo);

	auto pool = GetEmptyDescriptorPool();
	auto allocatorData = mData;
	auto device = mData->Device;

	vk::DescriptorSetAllocateInfo allocInfo{};
	allocInfo.setDescriptorPool(pool);
	allocInfo.setSetLayouts(Resource.Layout);

	Resource.Set = mData->Device->allocateDescriptorSets(allocInfo).front();

	mData->PoolBuffer.back().Inc();

	return Core::CreateRef<DescriptorResource>([allocatorData, pool](DescriptorResource& resource)
	{
		allocatorData->Device->destroyDescriptorSetLayout(resource.Layout);

		if (allocatorData->Info.Flags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
			allocatorData->Device->freeDescriptorSets(pool, resource.Set);
	}, Resource);
}

void VK_NAMESPACE::VK_CORE::DescriptorSetAllocator::Reserve(size_t NewSize)
{
	std::scoped_lock locker(mData->Lock);

	if (NewSize < mData->PoolBuffer.size())
		ScalePoolBufferCapacity(NewSize);
}

void VK_NAMESPACE::VK_CORE::DescriptorSetAllocator::ScalePoolBufferCapacity(size_t NewCap)
{
	if (NewCap < mData->PoolBuffer.size())
	{
		auto begin = mData->PoolBuffer.begin() + NewCap;
		auto end = mData->PoolBuffer.end();
		auto device = mData->Device;

		for (; begin != end; begin++)
		{
			device->destroyDescriptorPool(begin->Pool);
		}
	}

	mData->PoolBuffer.resize(NewCap);

	if (NewCap == 0)
		return;

	for (size_t i = NewCap - 1; i < mData->PoolBuffer.size(); i++)
	{
		mData->PoolBuffer[i].Pool = mBuilder.Build(mData->Info);
	}
}

vk::DescriptorPool VK_NAMESPACE::VK_CORE::DescriptorSetAllocator::GetEmptyDescriptorPool()
{
	std::scoped_lock locker(mData->Lock);

	auto& pools = mData->PoolBuffer;

	if (pools.empty())
		ScalePoolBufferCapacity(1);

	auto& lastCount = pools.back().AllocCount;

	if (lastCount.load() == mData->Info.BatchSize)
		ScalePoolBufferCapacity(mData->PoolBuffer.size() + 1);

	return mData->PoolBuffer.back().Pool;
}
