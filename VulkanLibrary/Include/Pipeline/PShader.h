#pragma once
#include "PipelineConfig.h"

VK_BEGIN

// TODO: We might have to adjust the ShaderInput and ShaderCompiler::Compile method a little
using ShaderMap = std::unordered_map<std::string, ShaderInput>;
using ShaderFiles = std::unordered_map<std::string, std::string>;

// Works as a state machine, so be careful when using it across multiple threads
// todo: may create its own pipeline layout and descriptor sets
// but then it will loss its independence from #VK_NAMESPACE::#Context
class PShader
{
public:
	PShader() : mEnv({ 
				glslang::EShTargetClientVersion::EShTargetVulkan_1_3,
				glslang::EShTargetLanguageVersion::EShTargetSpv_1_6,
				440 }) {}

	// User defined stuff...

	void SetCompilerConfig(const CompilerConfig& config)
	{ mEnv.SetConfig(config); }

	void AddMacro(const std::string& macro, const std::string& definition)
	{ mEnv.AddMacro(macro, definition); }

	void RemoveMacro(const std::string& macro)
	{ mEnv.RemoveMacro(macro); }

	void Clear() { mShaders.clear(); }

	// TODO: Implementing it only after adjusting that compiler method
	//std::string& operator[](const std::string& shaderStage) { mShaders[shaderStage]; }

	inline void SetShader(const std::string& stage, const std::string& shaderCode, 
		vkLib::OptimizerFlag opt = vkLib::OptimizerFlag::eO3);

	inline void SetFilepath(const std::string& stage, const std::string& filepath,
		vkLib::OptimizerFlag opt = vkLib::OptimizerFlag::eO3);

	// Compile shaders and store their result
	// TODO: Should this method be public or accessible to the builder only?
	inline std::vector<CompileError> CompileShaders();

	const PushConstantSubrangeInfos& GetPushConstantSubranges() const { return mPushConstantSubranges; }

	// Non-virtual getters
	std::pair<DescSetLayoutBindingMap, std::vector<vk::PushConstantRange>>
		GetPipelineLayoutInfo() const { return { mPipelineSetLayoutInfo, mPushConstantRanges }; }

	bool IsEmpty(uint32_t setNo, uint32_t bindingNo) const { return GetCount(setNo, bindingNo) == 0; }

	inline uint32_t GetCount(uint32_t setNo, uint32_t bindingNo) const;
	inline std::vector<ShaderSPIR_V> GetShaderByteCodes() const;

protected:
	// The fields below is going to be set by the CompileShaders function...
	DescSetLayoutBindingMap mPipelineSetLayoutInfo;
	PushConstantSubrangeInfos mPushConstantSubranges;
	std::vector<vk::PushConstantRange> mPushConstantRanges;

	std::vector<CompileResult> mCompileResults;

	CompilerEnvironment mEnv;

	glm::uvec3 mWorkGroupSize{};

	ShaderMap mShaders;

	template <typename BasePipeline>
	friend class BasicGraphicsPipeline;

	template <typename BasePipeline>
	friend class BasicComputePipeline;

	friend class PipelineBuilder;

	// TODO: Next is RayTracing Pipeline

	friend class Context;

private:
	// Helper methods...
	inline void SaveLayoutInfos(const CompileResult& result);
	inline void SavePushConstantRanges(const CompileResult& result);
	inline void SaveShaderMetaData(CompileResult Result);

	inline bool CheckIfSameBindings(const vk::DescriptorSetLayoutBinding& first,
		const vk::DescriptorSetLayoutBinding& second) const;
};

void PShader::SaveLayoutInfos(const CompileResult& result)
{
	for (const auto& [SetIndex, Bindings] : result.SetLayoutBindingsMap)
	{
		auto& SetInfo = mPipelineSetLayoutInfo[SetIndex];
		size_t SetInfoSize = SetInfo.size();

		SetInfo.reserve(SetInfo.size() + Bindings.size());

		for (const auto& Binding : Bindings)
		{
			bool Found = false;

			std::for_each(SetInfo.begin(), SetInfo.begin() + SetInfoSize,
				[this, &Binding, &Found, &result](vk::DescriptorSetLayoutBinding& current)
			{
				// TODO: Trigger an assert or throw an exception if two bindings 
				// have same binding and set index but difference name, types or data types!
				Found |= CheckIfSameBindings(current, Binding);
				if (Found)
					current.stageFlags |= result.MetaData.ShaderType;
			});

			if(!Found)
				SetInfo.emplace_back(Binding);
			else
			{
				// TODO:
			}
		}
	}
}

void PShader::SavePushConstantRanges(const CompileResult& result)
{
	// TODO: Push constants turned out to be more complicated...
	// Vulkan uses shared memory for push constants across all pipeline stages
	// Therefore, overlapping memory regions across multiple stages must be 
	// merged into a single large region to update them

	for (const auto& [name, range] : result.LayoutData.PushConstantSubrangeInfos)
	{
		mPushConstantSubranges[name] = range;
	}

	for (const auto& range : result.LayoutData.PushConstantsData)
	{
		mPushConstantRanges.push_back(range);
	}
}

std::vector<ShaderSPIR_V> PShader::GetShaderByteCodes() const
{
	std::vector<ShaderSPIR_V> byteCodes;

	for (const auto& result : mCompileResults)
		byteCodes.push_back(result.SPIR_V);

	return byteCodes;
}

inline void PShader::SaveShaderMetaData(CompileResult Result)
{
	if (Result.MetaData.ShaderType == vk::ShaderStageFlagBits::eCompute)
		mWorkGroupSize = Result.MetaData.WorkGroupSize;
}

std::vector<CompileError> PShader::CompileShaders()
{
	ShaderCompiler compiler(mEnv);

	mPipelineSetLayoutInfo.clear();
	mPushConstantSubranges.clear();
	mPushConstantRanges.clear();

	mCompileResults.clear();
	mCompileResults.reserve(mShaders.size());

	std::vector<CompileError> Errors;
	Errors.reserve(mShaders.size());

	for (const auto& [stage, shader] : mShaders)
	{
		auto Result = compiler.Compile(shader);
		Errors.push_back(Result.Error);

		// Retrieve the reflection results...
		SaveLayoutInfos(Result);
		SavePushConstantRanges(Result);
		SaveShaderMetaData(Result);

		mCompileResults.push_back(std::move(Result));
	}

	return Errors;
}

bool PShader::CheckIfSameBindings(const vk::DescriptorSetLayoutBinding& first, 
	const vk::DescriptorSetLayoutBinding& second) const
{
	return first.binding == second.binding && 
		first.descriptorCount == second.descriptorCount &&
		first.descriptorType == second.descriptorType;
}

void PShader::SetShader(const std::string& stage, const std::string& shaderCode, 
	vkLib::OptimizerFlag opt /*= vkLib::OptimizerFlag::eO3*/)
{
	mShaders[stage] = { shaderCode, ShaderCompiler::GetShaderStageFlag(stage), "", opt };
}

void PShader::SetFilepath(const std::string& stage, const std::string& filepath, vkLib::OptimizerFlag opt /*= vkLib::OptimizerFlag::eO3*/)
{
	mShaders[stage] = { "", ShaderCompiler::GetShaderStageFlag(stage), filepath, opt};
}

inline uint32_t PShader::GetCount(uint32_t setNo, uint32_t bindingNo) const
{
	auto bindings = mPipelineSetLayoutInfo.find(setNo);

	if (bindings == mPipelineSetLayoutInfo.end())
		return 0;

	for (const auto& binding : bindings->second)
	{
		if (binding.binding == bindingNo)
			return binding.descriptorCount;
	}

	return 0;
}

VK_END
