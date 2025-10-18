#include "Core/vkpch.h"
#include "Descriptors/DescriptorWriter.h"

#include "Core/Utils/Utils.h"

VK_BEGIN

DescriptorWriter::DescriptorWriter(DescriptorWriter&& Other) noexcept
	: mDevice(Other.mDevice), mDescriptorSets(std::move(Other.mDescriptorSets))
{
	Other.mDevice.Reset();
}

DescriptorWriter& DescriptorWriter::operator=(DescriptorWriter&& Other) noexcept
{
	mDevice = Other.mDevice;
	mDescriptorSets = std::move(Other.mDescriptorSets);

	Other.mDevice.Reset();
	return *this;
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const StorageBufferWriteInfo& bufferInfo) const
{
	vk::DescriptorBufferInfo bufferDescriptor;
	bufferDescriptor.buffer = bufferInfo.Buffer;
	bufferDescriptor.offset = bufferInfo.Offset;
	bufferDescriptor.range = bufferInfo.Range;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eStorageBuffer;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pBufferInfo = &bufferDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const UniformBufferWriteInfo& bufferInfo) const
{
	vk::DescriptorBufferInfo bufferDescriptor;
	bufferDescriptor.buffer = bufferInfo.Buffer;
	bufferDescriptor.offset = bufferInfo.Offset;
	bufferDescriptor.range = bufferInfo.Range;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eUniformBuffer;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pBufferInfo = &bufferDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const StorageImageWriteInfo& imageInfo) const
{
	vk::DescriptorImageInfo imageDescriptor;
	imageDescriptor.imageView = imageInfo.ImageView;
	imageDescriptor.imageLayout = imageInfo.ImageLayout;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eStorageImage;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pImageInfo = &imageDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const CombinedImageSamplerWriteInfo& samplerInfo) const
{
	vk::DescriptorImageInfo imageDescriptor;
	imageDescriptor.sampler = samplerInfo.Sampler;
	imageDescriptor.imageView = samplerInfo.ImageView;
	imageDescriptor.imageLayout = samplerInfo.ImageLayout;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pImageInfo = &imageDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const SampledImageWriteInfo& imageInfo) const
{
	vk::DescriptorImageInfo imageDescriptor;
	imageDescriptor.imageView = imageInfo.ImageView;
	imageDescriptor.imageLayout = imageInfo.ImageLayout;
	imageDescriptor.sampler = imageInfo.Sampler;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pImageInfo = &imageDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const SamplerWriteInfo& samplerInfo) const
{
	vk::DescriptorImageInfo imageDescriptor;
	imageDescriptor.sampler = samplerInfo.Sampler;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eSampler;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pImageInfo = &imageDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const InputAttachmentWriteInfo& attachmentInfo) const
{
	vk::DescriptorImageInfo imageDescriptor;
	imageDescriptor.imageView = attachmentInfo.ImageView;
	imageDescriptor.imageLayout = attachmentInfo.ImageLayout;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eInputAttachment;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pImageInfo = &imageDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const UniformTexelBufferWriteInfo& bufferInfo) const
{
	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eUniformTexelBuffer;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pTexelBufferView = &bufferInfo.BufferView;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const StorageTexelBufferWriteInfo& bufferInfo) const
{
	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eStorageTexelBuffer;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pTexelBufferView = &bufferInfo.BufferView;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}
void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const DynamicStorageBufferWriteInfo& bufferInfo) const
{
	vk::DescriptorBufferInfo bufferDescriptor;
	bufferDescriptor.buffer = bufferInfo.Buffer;
	bufferDescriptor.offset = bufferInfo.Offset;
	bufferDescriptor.range = bufferInfo.Range;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eStorageBufferDynamic;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pBufferInfo = &bufferDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

void DescriptorWriter::Update(
	const DescriptorLocation& info,
	const DynamicUniformBufferWriteInfo& bufferInfo) const
{
	vk::DescriptorBufferInfo bufferDescriptor;
	bufferDescriptor.buffer = bufferInfo.Buffer;
	bufferDescriptor.offset = bufferInfo.Offset;
	bufferDescriptor.range = bufferInfo.Range;

	vk::WriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet.dstSet = mDescriptorSets[info.SetIndex];
	writeDescriptorSet.dstBinding = info.Binding;
	writeDescriptorSet.dstArrayElement = info.ArrayIndex;
	writeDescriptorSet.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.pBufferInfo = &bufferDescriptor;

	mDevice->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
}

bool operator==(const DescriptorLocation& left, const DescriptorLocation& right)
{
	return left.SetIndex == right.SetIndex && 
		left.Binding == right.Binding && 
		left.ArrayIndex == right.ArrayIndex;
}

bool operator!=(const DescriptorLocation& left, const DescriptorLocation& right)
{
	return !(left == right);
}

size_t CreateHash(const DescriptorLocation& location)
{
	return Utils::CombineHash(location.SetIndex, Utils::CombineHash(location.Binding, location.ArrayIndex));
}

VK_END

