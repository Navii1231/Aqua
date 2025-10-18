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

	void InsertExecutionBarrier(
		vk::PipelineStageFlags srcStage,
		vk::PipelineStageFlags dstStage,
		vk::DependencyFlags dependencyFlags = vk::DependencyFlagBits());

	void InsertMemoryBarrier(
		vk::PipelineStageFlags srcStage,
		vk::PipelineStageFlags dstStage,
		vk::AccessFlags srcAccessMasks,
		vk::AccessFlags dstAccessMasks,
		vk::DependencyFlags dependencyFlags = vk::DependencyFlagBits());

	template <typename T>
	std::expected<bool, ShaderConstantError> SetShaderConstant(const std::string& name, const T& constant) const;

	virtual const PShader& GetShader() const { return mShaderInfo; }
	virtual void SetShader(const vkLib::PShader& shader) { mShaderInfo = shader; }

	virtual vk::PipelineBindPoint GetPipelineBindPoint() const { return (vk::PipelineBindPoint) -1; }

	template <typename WriteInfo>
	void UpdateDescriptor(const DescriptorLocation& location, const WriteInfo& writeInfo) const;

	vkLib::DescriptorWriter& GetDescriptorWriter() const { return mPipelineSpecs->DescWriter; }

	vk::CommandBuffer GetCommandBuffer() const { return mPipelineSpecs->PipelineCommands; }
	PipelineState GetPipelineState() const { return mPipelineSpecs->State.load(); }

	operator bool() const { return static_cast<bool>(mPipelineSpecs); }

protected:
	std::shared_ptr<BasicPipelineSpec> mPipelineSpecs;
	vkLib::PShader mShaderInfo;

	// Only PipelineBuilder is allowed to create an instance of this class
	friend class PipelineBuilder;

	BasicPipeline() = default;
	inline BasicPipeline(const ShaderFiles& shaders, const PreprocessorDirectives& directives = {});
	inline BasicPipeline(const PShader& shader);

	// NOTE: Calling BeginDefault MUST be accompanied by EndDefault at some point
	void BeginPipeline(vk::CommandBuffer commandBuffer) const;
	void EndPipeline() const;

	virtual PipelineLayoutData GetPipelineLayoutData() const { return {}; }

	friend VKLIB_API BasicPipeline Clone(Context ctx, const BasicPipeline& rsc);
};

inline void BasicPipeline::InsertExecutionBarrier(
	vk::PipelineStageFlags srcStage,
	vk::PipelineStageFlags dstStage,
	vk::DependencyFlags dependencyFlags /*= vk::DependencyFlagBits()*/)
{
	mPipelineSpecs->PipelineCommands.pipelineBarrier(srcStage, dstStage, dependencyFlags, {}, {}, {});
}

inline void BasicPipeline::InsertMemoryBarrier(vk::PipelineStageFlags srcStage, 
	vk::PipelineStageFlags dstStage, vk::AccessFlags srcAccessMasks, 
	vk::AccessFlags dstAccessMasks, vk::DependencyFlags dependencyFlags)
{
	vk::MemoryBarrier memoryBarrier{};

	memoryBarrier.setSrcAccessMask(srcAccessMasks);
	memoryBarrier.setDstAccessMask(dstAccessMasks);

	mPipelineSpecs->PipelineCommands.pipelineBarrier(srcStage, dstStage, 
		dependencyFlags, memoryBarrier, {}, {});
}

template <typename T>
std::expected<bool, ShaderConstantError> VK_NAMESPACE::BasicPipeline::SetShaderConstant(
	const std::string& name, const T& constant) const
{
	if (this->GetPipelineState() != PipelineState::eRecording)
	{
		ShaderConstantError error{};
		error.Info = "Pipeline must be in the recording state to set a shader constant (i.e within a Begin and End scope)!";
		error.Type = ShaderConstantErrorType::ePipelineNotInRecordingState;

		return std::unexpected(error);
	}

	vk::CommandBuffer commandBuffer = GetCommandBuffer();
	const PushConstantSubrangeInfos& subranges = GetShader().GetPushConstantSubranges();

	if (subranges.find(name) == subranges.end())
	{
		ShaderConstantError error{};
		error.Info = "Failed to find the push constant field \"" + name + "\" in the shader source code\n"
			"Note: If you turned on shader optimizations (vkLib::OptimizerFlag::eO3) "
			"or not using the field in the shader, it won't appear in the reflections";

		error.Type = ShaderConstantErrorType::eFailedToFindPushConstant;

		return std::unexpected(error);
	}

	const vk::PushConstantRange range = subranges.at(name);

	if (range.size != sizeof(constant))
	{
		ShaderConstantError error;
		error.Info = "Input field size of the push constant does not match with the expected size!\n"
			"Possible causes might be:\n"
			"* Alignment mismatch between GPU and CPU structs\n"
			"* Data type mismatch between shader and C++ declarations\n"
			"* The constant has been optimized away in the shader\n";

		error.Type = ShaderConstantErrorType::eSizeMismatch;

		return std::unexpected(error);
	}

	commandBuffer.pushConstants(GetPipelineLayoutData().Layout, range.stageFlags,
		range.offset, range.size, reinterpret_cast<const void*>(&constant));

	return true;
}

inline void BasicPipeline::BeginPipeline(vk::CommandBuffer commandBuffer) const
{
	_VK_ASSERT(mPipelineSpecs->State.load() == PipelineState::eNull,
		"Can't begin a pipeline scope as pipeline is already in recording state!");

	mPipelineSpecs->PipelineCommands = commandBuffer;
	mPipelineSpecs->State.store(PipelineState::eRecording);
}

inline void BasicPipeline::EndPipeline() const
{
	_VK_ASSERT(mPipelineSpecs->State.load() == PipelineState::eRecording, 
		"Can't end pipeline scope as no command buffer has been recorded!");

	mPipelineSpecs->PipelineCommands = nullptr;
	mPipelineSpecs->State.store(PipelineState::eNull);
}

BasicPipeline::BasicPipeline(const ShaderFiles& shaders, const PreprocessorDirectives& directives)
{
	vkLib::PShader shader{};

	for (const auto& [name, macro] : directives)
		shader.AddMacro(name, macro);

	for (const auto& [name, shaderString] : shaders)
		shader.SetFilepath(name, shaderString);

	auto errors = shader.CompileShaders();

	for (const auto& err : errors)
	{
		std::string message = "couldn't compile ";
		message += vkLib::ShaderCompiler::GetShaderStageString(err.ShaderStage);
		message += " shader" + err.Info;

		_STL_VERIFY(err.Type == ErrorType::eNone, message.c_str());
	}

	SetShader(shader);
}

BasicPipeline::BasicPipeline(const PShader& shader)
{
	SetShader(shader);
}

template <typename WriteInfo>
void VK_NAMESPACE::BasicPipeline::UpdateDescriptor(const DescriptorLocation& location, const WriteInfo& writeInfo) const
{
	mPipelineSpecs->DescWriter.Update(location, writeInfo);
}

VK_END

