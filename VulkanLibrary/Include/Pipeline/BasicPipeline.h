#pragma once
#include "PipelineConfig.h"
#include "../Descriptors/DescriptorWriter.h"
#include "PShader.h"

#include "../Core/Logger.h"

VK_BEGIN

class Context;

enum class PipelineState
{
	eNull            = 0,
	eRecording       = 1,
};

// NOTE: The context must be a copyable or movable entity
struct BasicPipelineSpec
{
	// Writing API to upload descriptors
	DescriptorWriter DescWriter;

	// Command buffer
	vk::CommandBuffer PipelineCommands{};

	// Pipeline state
	std::atomic<PipelineState> State{ PipelineState::eNull };

	BasicPipelineSpec(DescriptorWriter&& descWriter)
		: DescWriter(std::move(descWriter)) {}
};

enum class ShaderConstantErrorType
{
	eNone                                    = 0,
	ePipelineNotInRecordingState             = 1,
	eFailedToFindPushConstant                = 2,
	eSizeMismatch                            = 3,
};

struct ShaderConstantError
{
	std::string Info;
	ShaderConstantErrorType Type;
};

// Maybe updated and added more stuff later...
class BasicPipeline
{
public:
	virtual ~BasicPipeline() = default;

	inline void InsertExecutionBarrier(
		vk::PipelineStageFlags srcStage,
		vk::PipelineStageFlags dstStage,
		vk::DependencyFlags dependencyFlags = vk::DependencyFlagBits());

	inline void InsertMemoryBarrier(
		vk::PipelineStageFlags srcStage,
		vk::PipelineStageFlags dstStage,
		vk::AccessFlags srcAccessMasks,
		vk::AccessFlags dstAccessMasks,
		vk::DependencyFlags dependencyFlags = vk::DependencyFlagBits());

	template <typename T>
	std::expected<bool, ShaderConstantError> SetShaderConstant(const std::string& name, const T& constant) const;

	VKLIB_API std::expected<bool, ShaderConstantError> SetShaderConstant(const std::string& name, size_t size, const void* ptr) const;

	virtual const PShader& GetShader() const { return mShaderInfo; }
	virtual void SetShader(const vkLib::PShader& shader) { mShaderInfo = shader; }

	virtual vk::PipelineBindPoint GetPipelineBindPoint() const { return (vk::PipelineBindPoint) -1; }

	template <typename WriteInfo>
	void UpdateDescriptor(const DescriptorLocation& location, const WriteInfo& writeInfo) const;

	vkLib::DescriptorWriter& GetDescriptorWriter() const { return mPipelineSpecs->DescWriter; }

	vk::CommandBuffer GetCommandBuffer() const { return mPipelineSpecs->PipelineCommands; }
	PipelineState GetPipelineState() const { return mPipelineSpecs->State.load(); }

	virtual BasicPipeline* Clone(Context ctx) const;

	operator bool() const { return static_cast<bool>(mPipelineSpecs); }

protected:
	std::shared_ptr<BasicPipelineSpec> mPipelineSpecs;
	vkLib::PShader mShaderInfo;

	// Only PipelineBuilder is allowed to create an instance of this class
	friend class PipelineBuilder;

	BasicPipeline() = default;
	BasicPipeline(const ShaderFiles& shaders, const PreprocessorDirectives& directives = {});
	BasicPipeline(const PShader& shader);

	// NOTE: Calling BeginDefault MUST be accompanied by EndDefault at some point
	void BeginPipeline(vk::CommandBuffer commandBuffer) const;
	void EndPipeline() const;

	virtual PipelineLayoutData GetPipelineLayoutData() const { return {}; }

	friend VKLIB_API BasicPipeline* Clone(Context ctx, const BasicPipeline* rsc);
};

template <typename T>
std::expected<bool, ShaderConstantError> VK_NAMESPACE::BasicPipeline::SetShaderConstant(
	const std::string& name, const T& constant) const
{
	return SetShaderConstant(name, sizeof(constant), reinterpret_cast<const void*>(&constant));
}

inline void BasicPipeline::BeginPipeline(vk::CommandBuffer commandBuffer) const
{
	_VK_ASSERT(mPipelineSpecs->State.load() == PipelineState::eNull,
		"Can't begin a pipeline scope as pipeline is already in recording state!");

	mPipelineSpecs->PipelineCommands = commandBuffer;
	mPipelineSpecs->State.store(PipelineState::eRecording);
}

template <typename WriteInfo>
void VK_NAMESPACE::BasicPipeline::UpdateDescriptor(const DescriptorLocation& location, const WriteInfo& writeInfo) const
{
	mPipelineSpecs->DescWriter.Update(location, writeInfo);
}

VK_END

