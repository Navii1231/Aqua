#include "Core/vkpch.h"
#include "Pipeline/BasicPipeline.h"
#include "Device/Context.h"

VK_NAMESPACE::BasicPipeline::BasicPipeline(const PShader& shader)
{
	SetShader(shader);
}

VK_NAMESPACE::BasicPipeline::BasicPipeline(const ShaderFiles& shaders, const PreprocessorDirectives& directives)
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

void VK_NAMESPACE::BasicPipeline::EndPipeline() const
{
	_VK_ASSERT(mPipelineSpecs->State.load() == PipelineState::eRecording,
		"Can't end pipeline scope as no command buffer has been recorded!");

	mPipelineSpecs->PipelineCommands = nullptr;
	mPipelineSpecs->State.store(PipelineState::eNull);
}

void VK_NAMESPACE::BasicPipeline::InsertMemoryBarrier(vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, vk::AccessFlags srcAccessMasks, vk::AccessFlags dstAccessMasks, vk::DependencyFlags dependencyFlags)
{
	vk::MemoryBarrier memoryBarrier{};

	memoryBarrier.setSrcAccessMask(srcAccessMasks);
	memoryBarrier.setDstAccessMask(dstAccessMasks);

	mPipelineSpecs->PipelineCommands.pipelineBarrier(srcStage, dstStage,
		dependencyFlags, memoryBarrier, {}, {});
}

VK_NAMESPACE::BasicPipeline* VK_NAMESPACE::BasicPipeline::Clone(Context ctx) const
{
	auto bindPoint = GetPipelineBindPoint();
	auto pipelineBuilder = ctx.MakePipelineBuilder();

	switch (bindPoint)
	{
	case vk::PipelineBindPoint::eGraphics:
		return new GraphicsPipeline(pipelineBuilder.BuildGraphicsPipeline<GraphicsPipeline>(GetShader()));
	case vk::PipelineBindPoint::eCompute:
		return new ComputePipeline(pipelineBuilder.BuildComputePipeline<ComputePipeline>(GetShader()));
	case vk::PipelineBindPoint::eRayTracingKHR:
		_STL_VERIFY(false, "at cloning - pipeline unsupported yet");
		return {};
	default:
		_STL_VERIFY(false, "cloning bad pipeline");
		return {};
	}
}

void VK_NAMESPACE::BasicPipeline::InsertExecutionBarrier(vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, vk::DependencyFlags dependencyFlags /*= vk::DependencyFlagBits()*/)
{
	mPipelineSpecs->PipelineCommands.pipelineBarrier(srcStage, dstStage, dependencyFlags, {}, {}, {});
}

std::expected<bool, VK_NAMESPACE::ShaderConstantError> VK_NAMESPACE::BasicPipeline::SetShaderConstant(const std::string& name, size_t size, const void* ptr) const
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

	if (range.size != size)
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

	commandBuffer.pushConstants(GetPipelineLayoutData().Layout, range.stageFlags, range.offset, range.size, ptr);

	return true;
}
