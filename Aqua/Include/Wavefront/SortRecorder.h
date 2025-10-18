#pragma once
#include "LocalRadixSortPipeline.h"
#include "MergeSorterPipeline.h"

AQUA_BEGIN
PH_BEGIN

// Temporary for debugging...
#define _DEBUG_RAY_SORTER 0

// TODO: change required

template <typename CompType>
class SortRecorder
{
public:
	using SorterPipeline = MergeSorterPass<CompType>;
	using ArrayRef = typename SorterPipeline::ArrayRef;

public:
	SortRecorder(vkLib::PipelineBuilder builder, vkLib::ResourcePool manager);

	SortRecorder(const SortRecorder& Other);
	SortRecorder& operator =(const SortRecorder& Other);

	void InvalidateSorterPipeline(uint32_t workGroupSize);

	void SetBuffer(const vkLib::Buffer<ArrayRef>& buffer);
	uint32_t Run(vk::CommandBuffer commandBuffer);

	void CopyOutput(vkLib::Buffer<ArrayRef> buffer, uint32_t bufferIndex);

	void ResizeBuffer(uint32_t NewSize);
	vkLib::Buffer<ArrayRef> GetBuffer() const { return mBuffer; }

private:
	vkLib::Buffer<ArrayRef> mBuffer;

	uint32_t mWorkGroupSize = 0;

	SorterPipeline mMergePass;

	vkLib::PipelineBuilder mPipelineBuilder;
	vkLib::ResourcePool mResourcePool;

private:
	void CreateBuffer();
};

template<typename CompType>
inline SortRecorder<CompType>::SortRecorder(vkLib::PipelineBuilder builder, 
	vkLib::ResourcePool manager)
	: mPipelineBuilder(builder), mResourcePool(manager)
{
	CreateBuffer();
}

template<typename CompType>
inline SortRecorder<CompType>::SortRecorder(const SortRecorder& Other)
	: mPipelineBuilder(Other.mPipelineBuilder), mResourcePool(Other.mResourcePool), 
	mWorkGroupSize(Other.mWorkGroupSize)
{
	CreateBuffer();
	ResizeBuffer(static_cast<uint32_t>(Other.mBuffer.GetSize()));

	if (Other.mMergePass)
	{
		mMergePass = Other.mMergePass;

		mMergePass.SetBuffer(mBuffer);
		mMergePass.UpdateDescriptors();
	}
}

template<typename CompType>
inline SortRecorder<CompType>& SortRecorder<CompType>::operator=(const SortRecorder& Other)
{
	mWorkGroupSize = Other.mWorkGroupSize;

	CreateBuffer();
	mBuffer.Resize(Other.mBuffer.GetSize());

	if (Other.mMergePass)
	{
		mMergePass = Other.mMergePass;
		mMergePass.SetBuffer(mBuffer);
		mMergePass.UpdateDescriptors();
	}

	return *this;
}

template<typename CompType>
inline void SortRecorder<CompType>::InvalidateSorterPipeline(uint32_t workGroupSize)
{
	mWorkGroupSize = workGroupSize;

	mMergePass = mPipelineBuilder.BuildComputePipeline<SorterPipeline>(workGroupSize);

	mMergePass.SetBuffer(mBuffer);
	mMergePass.UpdateDescriptors();
}

template<typename CompType>
inline void SortRecorder<CompType>::SetBuffer(const vkLib::Buffer<ArrayRef>& buffer)
{
	mBuffer.Resize(2 * buffer.GetSize());

	vkLib::CopyBuffer(mBuffer, buffer);

	vk::BufferCopy copyBuffer{};

	copyBuffer.size = buffer.GetSize();
	copyBuffer.dstOffset = copyBuffer.size;

	//vkLib::CopyBufferRegions(mBuffer, buffer, { copyBuffer });

	mMergePass.SetBuffer(mBuffer);
}

template<typename CompType>
inline uint32_t SortRecorder<CompType>::Run(vk::CommandBuffer commandBuffer)
{
	uint32_t Size = static_cast<uint32_t>(mBuffer.GetSize() / 2);

	mMergePass.Begin(commandBuffer);
	mMergePass.Activate();

	/*   Push constant layout...
	* 
		layout(push_constant) uniform MetaData
		{
			uint pBufferSize;
			uint pActiveBuffer;
			uint pTreeDepth;
			uint pSectionLength;
		};
	*/

	mMergePass.SetShaderConstant("eCompute.MetaData.Index_0", (uint32_t) Size);

	uint32_t ActiveBuffer = 0;
	uint32_t TreeDepth = 1;

	for (uint32_t SequenceSize = 1; SequenceSize < Size; SequenceSize <<= 1)
	{
		// Slows it down, could use semaphores in its place
		mMergePass.InsertMemoryBarrier(vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eShaderRead);

		mMergePass.SetShaderConstant("eCompute.MetaData.Index_1", (uint32_t) ActiveBuffer);
		mMergePass.SetShaderConstant("eCompute.MetaData.Index_2", (uint32_t) TreeDepth++);
		mMergePass.SetShaderConstant("eCompute.MetaData.Index_3", (uint32_t) SequenceSize);

		mMergePass.Dispatch({ Size / mWorkGroupSize + 1, 1, 1 });

		ActiveBuffer = 1 - ActiveBuffer;
	}

	mMergePass.End();

	return ActiveBuffer;
}

template<typename CompType>
inline void SortRecorder<CompType>::CopyOutput(vkLib::Buffer<ArrayRef> buffer, uint32_t bufferIndex)
{
	vk::BufferCopy copyBuffer{};
	copyBuffer.srcOffset = bufferIndex * mBuffer.GetSize() / 2;
	copyBuffer.size = mBuffer.GetSize() / 2;

	vkLib::CopyBufferRegions(buffer, mBuffer, { copyBuffer });
}

template<typename CompType>
inline void SortRecorder<CompType>::ResizeBuffer(uint32_t NewSize)
{
	mBuffer.Resize(NewSize);

	mMergePass.SetBuffer(mBuffer);
	mMergePass.UpdateDescriptors();
}

template<typename CompType>
inline void SortRecorder<CompType>::CreateBuffer()
{
	vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eStorageBuffer;
	vk::MemoryPropertyFlags memProps = vk::MemoryPropertyFlagBits::eDeviceLocal;

#if _DEBUG_RAY_SORTER
	vk::MemoryPropertyFlags memProps = vk::MemoryPropertyFlagBits::eHostCoherent;
#endif

	mBuffer = mResourcePool.CreateBuffer<ArrayRef>(usage, memProps);
}

PH_END
AQUA_END
