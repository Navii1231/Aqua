#pragma once
#include "DescriptorsConfig.h"
#include "../Pipeline/PShader.h"

VK_BEGIN

// Struct to hold descriptor update information
struct DescriptorLocation {
	uint32_t SetIndex = 0;   // Descriptor set index
	uint32_t Binding = 0;    // Binding index
	uint32_t ArrayIndex = 0; // Array element index
};

bool operator ==(const DescriptorLocation& left, const DescriptorLocation& right);
bool operator !=(const DescriptorLocation& left, const DescriptorLocation& right);

size_t CreateHash(const DescriptorLocation& location);

class DescriptorWriter 
{
public:
	DescriptorWriter() = default;

	DescriptorWriter(const DescriptorWriter&) = delete;
	DescriptorWriter& operator =(const DescriptorWriter&) = delete;

	DescriptorWriter(DescriptorWriter&&) noexcept;
	DescriptorWriter& operator =(DescriptorWriter&&) noexcept;

	// Update descriptor with various types of write info
	VKLIB_API void Update(const DescriptorLocation& info, const StorageBufferWriteInfo& bufferInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const UniformBufferWriteInfo& bufferInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const StorageImageWriteInfo& imageInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const CombinedImageSamplerWriteInfo& samplerInfo)const;
	VKLIB_API void Update(const DescriptorLocation& info, const SampledImageWriteInfo& imageInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const SamplerWriteInfo& samplerInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const InputAttachmentWriteInfo& attachmentInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const UniformTexelBufferWriteInfo& bufferInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const StorageTexelBufferWriteInfo& bufferInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const DynamicStorageBufferWriteInfo& bufferInfo) const;
	VKLIB_API void Update(const DescriptorLocation& info, const DynamicUniformBufferWriteInfo& bufferInfo) const;

private:
	Core::Ref<vk::Device> mDevice; // Vulkan device handle
	std::vector<vk::DescriptorSet> mDescriptorSets; // Descriptor sets to update

	DescriptorWriter(Core::Ref<vk::Device> device, const std::vector<vk::DescriptorSet>& descriptorSets)
		: mDevice(device), mDescriptorSets(descriptorSets) {}

	friend class PipelineBuilder;
};
VK_END
