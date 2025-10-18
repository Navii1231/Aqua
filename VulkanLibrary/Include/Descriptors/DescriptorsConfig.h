#pragma once
#include "../Core/Config.h"
#include "../Core/Ref.h"

VK_BEGIN

struct DescriptorResource
{
	vk::DescriptorSetLayout Layout;
	vk::DescriptorSet Set;
};

VK_CORE_BEGIN

struct DescriptorSetAllocatorInfo
{
	vk::DescriptorPoolCreateFlags Flags;
	std::unordered_set<vk::DescriptorType> Types;
	uint32_t BatchSize = 100;
	uint32_t BindingCount = 100;
};

struct DescriptorPoolData
{
	vk::DescriptorPool Pool;
	std::atomic<uint32_t> AllocCount{ 0 };

	DescriptorPoolData() = default;

	DescriptorPoolData(const DescriptorPoolData& Other)
		: Pool(Other.Pool) {}

	DescriptorPoolData& operator =(const DescriptorPoolData& Other) 
	{ Pool = Other.Pool; return *this; }

	void Inc() { AllocCount++; }
	void Dec() { AllocCount--; }
};

struct DescriptorSetAllocatorData
{
	std::vector<DescriptorPoolData> PoolBuffer;
	DescriptorSetAllocatorInfo Info;

	std::mutex Lock;

	Core::Ref<vk::Device> Device;

	DescriptorSetAllocatorData() = default;

	DescriptorSetAllocatorData(const DescriptorSetAllocatorData& Other)
		: PoolBuffer(Other.PoolBuffer), Info(Other.Info), Device(Other.Device) {}
};

struct DescriptorSetAllocatorDataDeleter
{
	void operator ()(DescriptorSetAllocatorData& data) const
	{
		for (auto& poolData : data.PoolBuffer)
			data.Device->destroyDescriptorPool(poolData.Pool);
	}
};

VK_CORE_END

// Struct for storage buffer information
struct StorageBufferWriteInfo {
	vk::Buffer Buffer;
	vk::DeviceSize Offset = 0;
	vk::DeviceSize Range = VK_WHOLE_SIZE;
};

// Struct for uniform buffer information
struct UniformBufferWriteInfo {
	vk::Buffer Buffer;
	vk::DeviceSize Offset = 0;
	vk::DeviceSize Range = VK_WHOLE_SIZE;
};

// Struct for storage image information
struct StorageImageWriteInfo {
	vk::ImageView ImageView;
	vk::ImageLayout ImageLayout;
};

// Struct for combined image sampler information
struct CombinedImageSamplerWriteInfo {
	vk::ImageView ImageView;
	vk::Sampler Sampler;
	vk::ImageLayout ImageLayout;
};

// Struct for sampled image information
struct SampledImageWriteInfo {
	vk::ImageView ImageView;
	vk::ImageLayout ImageLayout;
	vk::Sampler Sampler;
};

// Struct for sampler information
struct SamplerWriteInfo {
	vk::Sampler Sampler;
};

// Struct for input attachment information
struct InputAttachmentWriteInfo {
	vk::ImageView ImageView;
	vk::ImageLayout ImageLayout;
};

// Struct for uniform texel buffer information
struct UniformTexelBufferWriteInfo {
	vk::BufferView BufferView;
};

// Struct for storage texel buffer information
struct StorageTexelBufferWriteInfo {
	vk::BufferView BufferView;
};

// Struct for dynamic storage buffer information
struct DynamicStorageBufferWriteInfo {
	vk::Buffer Buffer;
	vk::DeviceSize Offset;
	vk::DeviceSize Range;
};

// Struct for dynamic uniform buffer information
struct DynamicUniformBufferWriteInfo {
	vk::Buffer Buffer;
	vk::DeviceSize Offset;
	vk::DeviceSize Range;
};

VK_END
