#pragma once
#include "PipelineConfig.h"
#include "PShader.h"
#include "BasicPipeline.h"

#include "../Core/Logger.h"

VK_BEGIN

struct ComputePipelineHandles : public PipelineHandles, public PipelineInfo
{
	glm::uvec3 WorkGroupSize = { 0, 0, 0 }; // By default, invalid workgroup
};

template <typename BasePipeline>
class BasicComputePipeline : public BasePipeline
{
public:
	BasicComputePipeline() = default;
	BasicComputePipeline(const ShaderFiles& shaders, const PreprocessorDirectives& directives = {})
		: BasePipeline(shaders, directives) {}

	BasicComputePipeline(const PShader& shader)
		: BasePipeline(shader) {}

	virtual ~BasicComputePipeline() = default;

	virtual void Begin(vk::CommandBuffer commandBuffer) const;

	// Binds the pipeline to a compute bind point
	// In contrast to graphics pipeline, compute pipeline might be dispatched
	// multiple times (asynchronously) within a single command buffer
	// This function is separately provided to support that functionality
	void Activate() const;

	// Async Dispatch...
	void Dispatch(const glm::uvec3& workGroups) const;

	virtual void End() const;

	virtual vk::PipelineBindPoint GetPipelineBindPoint() const override { return vk::PipelineBindPoint::eCompute; }
	virtual PipelineLayoutData GetPipelineLayoutData() const override { return mHandles->LayoutData; }

	glm::uvec3 GetWorkGroupSize() const { return mHandles->WorkGroupSize; }

	explicit operator bool() const { return static_cast<bool>(mHandles); }

private:
	Core::Ref<ComputePipelineHandles> mHandles;

	friend class PipelineBuilder;

	template <typename _Rsc>
	_Rsc Clone(Context ctx, const _Rsc& rsc);
};

using ComputePipeline = BasicComputePipeline<BasicPipeline>;

template<typename BasePipeline>
inline void BasicComputePipeline<BasePipeline>::Begin(vk::CommandBuffer commandBuffer) const
{
	this->BeginPipeline(commandBuffer);
}

template<typename BasePipeline>
inline void BasicComputePipeline<BasePipeline>::Activate() const
{
	vk::CommandBuffer commandBuffer = ((BasePipeline*) this)->GetCommandBuffer();
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, mHandles->Handle);
}

template<typename BasePipeline>
inline void BasicComputePipeline<BasePipeline>::Dispatch(const glm::uvec3& WorkGroups) const
{
	vk::CommandBuffer commandBuffer = ((BasePipeline*) this)->GetCommandBuffer();

	if (!mHandles->SetCache.empty())
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
			mHandles->LayoutData.Layout, 0, mHandles->SetCache, nullptr);

	commandBuffer.dispatch(WorkGroups.x, WorkGroups.y, WorkGroups.z);
}

template<typename BasePipeline>
inline void BasicComputePipeline<BasePipeline>::End() const
{
	this->EndPipeline();
}

VK_END
