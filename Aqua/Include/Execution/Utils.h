#pragma once
#include "GraphConfig.h"

AQUA_BEGIN
EXEC_BEGIN

template <typename _Pipeline>
void UpdateSampledImage(const _Pipeline& pipeline, const vkLib::DescriptorLocation& location, vkLib::ImageView imageView, vkLib::Core::Ref<vk::Sampler> sampler)
{
	const auto& shader = pipeline.GetShader();

	vkLib::SampledImageWriteInfo sampledImageInfo{};
	sampledImageInfo.ImageLayout = vk::ImageLayout::eGeneral;
	sampledImageInfo.ImageView = imageView.GetNativeHandle();
	sampledImageInfo.Sampler = *sampler;

	if (!shader.IsEmpty(location.SetIndex, location.Binding))
		pipeline.UpdateDescriptor(location, sampledImageInfo);
};

template <typename _Pipeline>
void UpdateStorageImage(const _Pipeline& pipeline, const vkLib::DescriptorLocation& location, vkLib::ImageView imageView)
	{
		const auto& shader = pipeline.GetShader();

		vkLib::StorageImageWriteInfo sampledImageInfo{};
		sampledImageInfo.ImageLayout = vk::ImageLayout::eGeneral;
		sampledImageInfo.ImageView = imageView.GetNativeHandle();

		if (!shader.IsEmpty(location.SetIndex, location.Binding))
			pipeline.UpdateDescriptor(location, sampledImageInfo);
	};

template <typename _Pipeline>
void UpdateStorageBuffer(const _Pipeline& pipeline, const vkLib::DescriptorLocation& location, vkLib::GenericBuffer buffer)
	{
		const auto& shader = pipeline.GetShader();

		vkLib::StorageBufferWriteInfo storageBuffer{};
		storageBuffer.Buffer = buffer.GetNativeHandles().Handle;

		if (!shader.IsEmpty(location.SetIndex, location.Binding) && storageBuffer.Buffer)
			pipeline.UpdateDescriptor(location, storageBuffer);
	};

template <typename _Pipeline>
void UpdateUniformBuffer(const _Pipeline& pipeline, const vkLib::DescriptorLocation& location, vkLib::GenericBuffer buffer)
	{
		const auto& shader = pipeline.GetShader();

		vkLib::UniformBufferWriteInfo uniformBuffer{};
		uniformBuffer.Buffer = buffer.GetNativeHandles().Handle;

		if (!shader.IsEmpty(location.SetIndex, location.Binding) && uniformBuffer.Buffer)
			pipeline.UpdateDescriptor(location, uniformBuffer);
	};


EXEC_END
AQUA_END
