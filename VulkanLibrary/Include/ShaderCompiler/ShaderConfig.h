#pragma once
#include "../Core/Config.h"
#include "glslang/Public/ShaderLang.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/Public/ResourceLimits.h"

VK_BEGIN

// Set number --> all of its the bindings
using DescSetLayoutBindingMap = std::unordered_map<uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>;
using PushConstantSubrangeInfos = std::unordered_map<std::string, vk::PushConstantRange>;
using PreprocessorDirectives = std::unordered_map<std::string, std::string>;

enum class ErrorType
{
	eNone            = 0,
	ePreprocess      = 1,
	eParsing         = 2,
	eLinking         = 3,
};

enum OptimizerFlag
{
	eNone            = 0,
	eO1              = 1,
	eO2              = 2,
	eO3              = 3,
};

struct CompileError
{
	ErrorType Type = ErrorType::eNone;
	vk::ShaderStageFlagBits ShaderStage;
	std::string Info;
	std::string DebugInfo;
	std::string SrcCode;
	std::string PreprocessedCode;

	std::string FilePath;
};

struct ShaderSPIR_V
{
	std::vector<uint32_t> ByteCode;
	vk::ShaderStageFlagBits Stage;
};

struct ShaderInput
{
	std::string SrcCode;
	vk::ShaderStageFlagBits Stage;

	std::string FilePath;

	OptimizerFlag OptimizationFlag = OptimizerFlag::eNone;
};

struct CompilerConfig
{
	glslang::EShTargetClientVersion VulkanVersion;
	glslang::EShTargetLanguageVersion SPV_Version = glslang::EShTargetSpv_1_5;
	int GlslVersion = 330;
};

struct DescriptorInfo
{
	uint32_t SetIndex = -1;
	uint32_t BindingIndex = -1;
	std::string Name;
	spirv_cross::SPIRType DescReflectionInfo;
	vk::DescriptorType DescType;
};

struct LayoutInfo
{
	std::vector<DescriptorInfo> DescInfos;
	std::vector<vk::PushConstantRange> PushConstantsData;
	PushConstantSubrangeInfos PushConstantSubrangeInfos;
};

struct ShaderMetaData
{
	vk::ShaderStageFlagBits ShaderType;
	glm::uvec3 WorkGroupSize{}; // For compute shader...
};

struct CompileResult
{
	CompilerConfig Config;
	ShaderMetaData MetaData;

	LayoutInfo LayoutData;
	DescSetLayoutBindingMap SetLayoutBindingsMap;

	CompileError Error;
	ShaderSPIR_V SPIR_V;
};

VK_END
