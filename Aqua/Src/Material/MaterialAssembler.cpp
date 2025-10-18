#include "Core/Aqpch.h"
#include "Material/MaterialAssembler.h"
#include "Utils/CompilerErrorChecker.h"

std::string AQUA_NAMESPACE::MAT_NAMESPACE::MaterialAssembler::sDeferGFXVertexShader = R"(
#version 440 core
layout(location = 0) out vec2 vTexCoords;

vec2 positions[6] = vec2[](

	vec2(-1.0, -1.0),
	vec2(1.0, -1.0),
	vec2(1.0, 1.0),
	vec2(1.0, 1.0),
	vec2(-1.0, 1.0),
	vec2(-1.0, -1.0)

	);

vec2 texCoords[6] = vec2[](

	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(1.0, 1.0),
	vec2(1.0, 1.0),
	vec2(0.0, 1.0),
	vec2(0.0, 0.0)

	);

void main()
{
	gl_Position.xy = positions[gl_VertexIndex];
	gl_Position.z = 1.0;
	gl_Position.w = 1.0;

	vTexCoords = texCoords[gl_VertexIndex];
}
)";

std::expected<AQUA_NAMESPACE::MAT_NAMESPACE::Material, vkLib::CompileError>
	AQUA_NAMESPACE::MAT_NAMESPACE::MaterialAssembler::ConstructRayTracingMaterial(
	const std::string& code, const vkLib::PreprocessorDirectives& directives /*= {}*/)
{
	auto OptimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	OptimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader{};
	shader.SetShader("eCompute", code, OptimizerFlag);
	
	for (const auto& [name, directive] : directives)
		shader.AddMacro(name, directive);

	auto errors = shader.CompileShaders();

	CompileErrorChecker checker("D:\\Dev\\Aqua\\Aqua\\Assets\\Shaders\\Logging\\Error.glsl");
	checker.GetError(errors[0]);
	checker.AssertOnError(errors);

	for (const auto& error : errors)
	{
		if (error.Type != vkLib::ErrorType::eNone)
			return std::unexpected(error);
	}

	EXEC_NAMESPACE::Operation execOp{};

	vkLib::ComputePipeline fakePipeline;
	fakePipeline.SetShader(shader);

	execOp.Cmp = MakeRef<vkLib::ComputePipeline>(mPipelineBuilder.BuildComputePipeline<vkLib::ComputePipeline>(fakePipeline));
	execOp.States = EXEC_NAMESPACE::OpType::eCompute;

	return execOp;
}

std::expected<AQUA_NAMESPACE::MAT_NAMESPACE::Material, vkLib::CompileError> 
	AQUA_NAMESPACE::MAT_NAMESPACE::MaterialAssembler::ConstructDeferGFXMaterial(
	const std::string& code, vkLib::GraphicsPipelineConfig config, const vkLib::PreprocessorDirectives& directives /*= {}*/)
{
	auto OptimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	OptimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader{};
	shader.SetShader("eVertex", sDeferGFXVertexShader, OptimizerFlag);
	shader.SetShader("eFragment", code, OptimizerFlag);

	for (const auto& [name, directive] : directives)
		shader.AddMacro(name, directive);

	auto errors = shader.CompileShaders();

	CompileErrorChecker checker("D:\\Dev\\Aqua\\Aqua\\Include\\DeferredRenderer\\Shaders\\Logging\\ShaderError.glsl");

	checker.GetError(errors[0]);
	checker.GetError(errors[1]);
	checker.AssertOnError(errors);

	for (const auto& error : errors)
	{
		if (error.Type != vkLib::ErrorType::eNone)
			return std::unexpected(error);
	}

	EXEC_NAMESPACE::Operation execOp{};

	vkLib::GraphicsPipeline fakePipeline;
	fakePipeline.SetShader(shader);
	fakePipeline.SetConfig(config);

	vkLib::GraphicsPipeline pipeline = mPipelineBuilder.BuildGraphicsPipeline<vkLib::GraphicsPipeline>(fakePipeline);

	execOp.GFX = MakeRef<vkLib::GraphicsPipeline>(pipeline);
	execOp.States = EXEC_NAMESPACE::OpType::eGraphics;

	return execOp;
}

std::expected<AQUA_NAMESPACE::MAT_NAMESPACE::Material, vkLib::CompileError> AQUA_NAMESPACE::MAT_NAMESPACE::
	MaterialAssembler::ConstructDeferCmpMaterial(const std::string& code, 
		const vkLib::PreprocessorDirectives& directives /*= {}*/)
{
	auto OptimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	OptimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader{};
	shader.SetShader("eCompute", code, OptimizerFlag);

	for (const auto& [name, directive] : directives)
		shader.AddMacro(name, directive);

	auto errors = shader.CompileShaders();

	CompileErrorChecker checker("D:\\Dev\\Aqua\\Aqua\\Include\\DeferredRenderer\\Shaders\\Logging\\ShaderError.glsl");

	checker.GetError(errors[0]);
	checker.AssertOnError(errors);

	for (const auto& error : errors)
	{
		if (error.Type != vkLib::ErrorType::eNone)
			return std::unexpected(error);
	}

	EXEC_NAMESPACE::Operation execOp{};

	vkLib::ComputePipeline fakePipeline;
	fakePipeline.SetShader(shader);

	vkLib::ComputePipeline pipeline = mPipelineBuilder.BuildComputePipeline<vkLib::ComputePipeline>(fakePipeline);

	execOp.Cmp = MakeRef<vkLib::ComputePipeline>(pipeline);
	execOp.States = EXEC_NAMESPACE::OpType::eCompute;

	return execOp;
}
