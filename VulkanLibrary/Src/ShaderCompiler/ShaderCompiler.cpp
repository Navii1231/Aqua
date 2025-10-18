#include "Core/vkpch.h"
#include "ShaderCompiler/ShaderCompiler.h"

#include "ShaderCompiler/Lexer.h"

VK_BEGIN

template <typename MapAToB, typename MapBToA = std::unordered_map<typename MapAToB::mapped_type, typename MapAToB::key_type>>
MapBToA CreateInverseMap(const MapAToB& map)
{
	MapBToA ret;

	for (const auto& [key, val] : map)
	{
		ret[val] = key;
	}

	return ret;
}

struct ProcessInitializer
{
	ProcessInitializer()
	{
		glslang::InitializeProcess();
		InitMaps();
	}

	~ProcessInitializer()
	{
		glslang::FinalizeProcess();
	}

	DescSetLayoutBindingMap GetSetBindings(const std::vector<DescriptorInfo>& Infos, 
		vk::ShaderStageFlags Stages) const
	{
		DescSetLayoutBindingMap SetBindingsMap;
		SetBindingsMap.reserve(Infos.size());

		for (const auto& desc : Infos)
		{
			auto& Bindings = SetBindingsMap[desc.SetIndex];
			auto& RecentBinding = Bindings.emplace_back();

			RecentBinding.binding = desc.BindingIndex;
			RecentBinding.descriptorCount = 1;
			RecentBinding.descriptorType = desc.DescType;
			RecentBinding.stageFlags = Stages;
		}

		return SetBindingsMap;
	}

	std::string GetShaderStageString(vk::ShaderStageFlagBits stage) const
	{
		return mStageToString.at(stage);
	}

	vk::ShaderStageFlagBits GetShaderStageFlag(const std::string& stage) const
	{
		return mStringToStage.at(stage);
	}

	std::unordered_map<vk::ShaderStageFlagBits, EShLanguage> mVulkanToEShStage;
	std::unordered_map<EShLanguage, vk::ShaderStageFlagBits> mEShToVulkanStage;

	std::unordered_map<vk::ShaderStageFlagBits, std::string> mStageToString;
	std::unordered_map<std::string, vk::ShaderStageFlagBits> mStringToStage;

private:

	void InitMaps()
	{
		mEShToVulkanStage[EShLangVertex] = vk::ShaderStageFlagBits::eVertex;
		mEShToVulkanStage[EShLangTessControl] = vk::ShaderStageFlagBits::eTessellationControl;
		mEShToVulkanStage[EShLangTessEvaluation] = vk::ShaderStageFlagBits::eTessellationEvaluation;
		mEShToVulkanStage[EShLangGeometry] = vk::ShaderStageFlagBits::eGeometry;
		mEShToVulkanStage[EShLangFragment] = vk::ShaderStageFlagBits::eFragment;
		mEShToVulkanStage[EShLangCompute] = vk::ShaderStageFlagBits::eCompute;
		mEShToVulkanStage[EShLangRayGen] = vk::ShaderStageFlagBits::eRaygenKHR;
		mEShToVulkanStage[EShLangRayGenNV] = vk::ShaderStageFlagBits::eRaygenNV;
		mEShToVulkanStage[EShLangIntersect] = vk::ShaderStageFlagBits::eIntersectionKHR;
		mEShToVulkanStage[EShLangIntersectNV] = vk::ShaderStageFlagBits::eIntersectionNV;
		mEShToVulkanStage[EShLangAnyHit] = vk::ShaderStageFlagBits::eAnyHitKHR;
		mEShToVulkanStage[EShLangAnyHitNV] = vk::ShaderStageFlagBits::eAnyHitNV;
		mEShToVulkanStage[EShLangClosestHit] = vk::ShaderStageFlagBits::eClosestHitKHR;
		mEShToVulkanStage[EShLangClosestHitNV] = vk::ShaderStageFlagBits::eClosestHitNV;
		mEShToVulkanStage[EShLangMiss] = vk::ShaderStageFlagBits::eMissKHR;
		mEShToVulkanStage[EShLangMissNV] = vk::ShaderStageFlagBits::eMissNV;
		mEShToVulkanStage[EShLangCallable] = vk::ShaderStageFlagBits::eCallableKHR;
		mEShToVulkanStage[EShLangCallableNV] = vk::ShaderStageFlagBits::eCallableNV;
		mEShToVulkanStage[EShLangTask] = vk::ShaderStageFlagBits::eTaskEXT;
		mEShToVulkanStage[EShLangTaskNV] = vk::ShaderStageFlagBits::eTaskNV;
		mEShToVulkanStage[EShLangMesh] = vk::ShaderStageFlagBits::eMeshEXT;
		mEShToVulkanStage[EShLangMeshNV] = vk::ShaderStageFlagBits::eMeshNV;

		mStageToString[vk::ShaderStageFlagBits::eVertex] = "eVertex";
		mStageToString[vk::ShaderStageFlagBits::eTessellationControl] = "eTessellationControl";
		mStageToString[vk::ShaderStageFlagBits::eTessellationEvaluation] = "eTessellationEvaluation";
		mStageToString[vk::ShaderStageFlagBits::eGeometry] = "eGeometry";
		mStageToString[vk::ShaderStageFlagBits::eFragment] = "eFragment";
		mStageToString[vk::ShaderStageFlagBits::eCompute] = "eCompute";
		mStageToString[vk::ShaderStageFlagBits::eRaygenKHR] = "eRaygenKHR";
		mStageToString[vk::ShaderStageFlagBits::eAnyHitKHR] = "eAnyHitKHR";
		mStageToString[vk::ShaderStageFlagBits::eClosestHitKHR] = "eClosestHitKHR";
		mStageToString[vk::ShaderStageFlagBits::eMissKHR] = "eMissKHR";
		mStageToString[vk::ShaderStageFlagBits::eIntersectionKHR] = "eIntersectionKHR";
		mStageToString[vk::ShaderStageFlagBits::eCallableKHR] = "eCallableKHR";
		mStageToString[vk::ShaderStageFlagBits::eTaskEXT] = "eTaskEXT";
		mStageToString[vk::ShaderStageFlagBits::eMeshEXT] = "eMeshEXT";
		mStageToString[vk::ShaderStageFlagBits::eSubpassShadingHUAWEI] = "eSubpassShadingHUAWEI";
		mStageToString[vk::ShaderStageFlagBits::eClusterCullingHUAWEI] = "eClusterCullingHUAWEI";

		mVulkanToEShStage = CreateInverseMap(mEShToVulkanStage);
		mStringToStage = CreateInverseMap(mStageToString);
	}

} const sCompilerInitializer;

VK_END

std::string VK_NAMESPACE::ShaderCompiler::GetShaderStageString(vk::ShaderStageFlagBits flag)
{
	return sCompilerInitializer.GetShaderStageString(flag);
}

vk::ShaderStageFlagBits VK_NAMESPACE::ShaderCompiler::GetShaderStageFlag(const std::string& shaderStage)
{
	return sCompilerInitializer.GetShaderStageFlag(shaderStage);
}

VK_NAMESPACE::ShaderCompiler::ShaderCompiler(const ShaderCompiler& other)
	: mEnvironment(other.mEnvironment) {}

VK_NAMESPACE::ShaderCompiler& VK_NAMESPACE::ShaderCompiler::operator=(const ShaderCompiler& other)
{
	mEnvironment = other.mEnvironment;

	return *this;
}

VK_NAMESPACE::CompileResult VK_NAMESPACE::ShaderCompiler::Compile(ShaderInput&& Input)
{
	CompileResult Result;
	Result.Config = mEnvironment.GetConfig();
	Result.Error.ShaderStage = Input.Stage;

	if (!Input.FilePath.empty())
		ReadAndPreprocess(Result, Input);
	else
		Result.Error.SrcCode = std::move(Input.SrcCode);

	if (Result.Error.Type != ErrorType::eNone)
		return Result;

	Result.Error.FilePath = std::move(Input.FilePath);

	auto EShStage = ConvertShaderStage(Input.Stage);

	if (!PreprocessShader(Result, EShStage))
		return Result;

	if (!ParseShader(Result, EShStage))
		return Result;

	OptimizeCode(Result, Input.OptimizationFlag);

	ReflectDescriptorLayouts(Result);
	ReflectShaderMetaData(Result);

	return Result;
}

VK_NAMESPACE::CompileResult VK_NAMESPACE::ShaderCompiler::PreprocessString(
	const std::string& shaderString, vk::ShaderStageFlagBits stage)
{
	auto EShStage = ConvertShaderStage(stage);

	CompileResult result;
	result.Config = mEnvironment.GetConfig();
	result.Error.ShaderStage = stage;

	result.Error.SrcCode = shaderString;

	PreprocessShader(result, EShStage);

	return result;
}

vk::ShaderStageFlagBits VK_NAMESPACE::ShaderCompiler::ConvertShaderStage(EShLanguage Stage)
{
	return sCompilerInitializer.mEShToVulkanStage.at(Stage);
}

EShLanguage VK_NAMESPACE::ShaderCompiler::ConvertShaderStage(vk::ShaderStageFlagBits Stage)
{
	return sCompilerInitializer.mVulkanToEShStage.at(Stage);
}

VK_NAMESPACE::CompileResult VK_NAMESPACE::ShaderCompiler::Compile(const ShaderInput& Input)
{
	auto Copy = Input;
	return Compile(std::move(Copy));
}

bool VK_NAMESPACE::ShaderCompiler::PreprocessShader(
	CompileResult& ResultRef, EShLanguage Stage)
{
	auto TShader = MakeGLSLangShader(mEnvironment, Stage);
	const char* Source = ResultRef.Error.SrcCode.c_str();

	TShader.setStrings(&Source, 1);

	std::string Preamble;

	for (const auto& [macro, define] : mEnvironment.GetMacroDefines())
	{
		Preamble += "#define " + macro + " " + define + "\n";
	}

	TShader.setPreamble(Preamble.c_str());

	glslang::TShader::ForbidIncluder basicIncluder = glslang::TShader::ForbidIncluder();

	bool Success = TShader.preprocess(GetDefaultResources(), 100, EProfile::ENoProfile,
		false, true, EShMsgDefault, &ResultRef.Error.PreprocessedCode, basicIncluder);

	if (!Success)
	{
		ResultRef.Error.DebugInfo = TShader.getInfoDebugLog();
		ResultRef.Error.Info = TShader.getInfoLog();
		ResultRef.Error.Type = ErrorType::ePreprocess;
	}

	return Success;
}

bool VK_NAMESPACE::ShaderCompiler::ParseShader(CompileResult& Result, EShLanguage Stage)
{
	auto TShader = MakeGLSLangShader(mEnvironment, Stage);
	const char* Source = Result.Error.PreprocessedCode.c_str();

	TShader.setStrings(&Source, 1);

	std::string Preamble;

	for (const auto& [macro, define] : mEnvironment.GetMacroDefines())
	{
		Preamble += "#define " + macro + " " + define + "\n";
	}

	TShader.setPreamble(Preamble.c_str());

	glslang::TShader::ForbidIncluder basicIncluder = glslang::TShader::ForbidIncluder();

	bool Success = TShader.parse(GetDefaultResources(), 100, EProfile::ENoProfile,
		false, true, EShMsgDefault, basicIncluder);

	if (!Success)
	{
		Result.Error.DebugInfo = TShader.getInfoDebugLog();
		Result.Error.Info = TShader.getInfoLog();
		Result.Error.Type = ErrorType::eParsing;

		return Success;
	}

	return GenerateSPIR_V(Result, Stage, TShader);
}

bool VK_NAMESPACE::ShaderCompiler::GenerateSPIR_V(
	CompileResult& Result, EShLanguage Stage, glslang::TShader& shader)
{
	glslang::TProgram Program;
	Program.addShader(&shader);

	bool Success = Program.link(EShMsgDefault);

	if (!Success)
	{
		Result.Error.Info = Program.getInfoLog();
		Result.Error.DebugInfo = Program.getInfoDebugLog();
		Result.Error.Type = ErrorType::eLinking;

		return Success;
	}

	glslang::GlslangToSpv(
		*Program.getIntermediate(Stage), Result.SPIR_V.ByteCode);

	Result.SPIR_V.Stage = ConvertShaderStage(Stage);

	return Success;
}

void VK_NAMESPACE::ShaderCompiler::ResetInternal(const CompilerConfig& new_in)
{
	// Reset all the fields...
}

void VK_NAMESPACE::ShaderCompiler::OptimizeCode(CompileResult& Result, OptimizerFlag Flag)
{
	/********************* OPTIMIZATIONS **************************/
	// Optimize for performance as much as we can for now

	if (Flag != OptimizerFlag::eNone)
	{

		std::vector<uint32_t> OptimizedCode;

		spvtools::Optimizer Optimizer(spv_target_env::SPV_ENV_VULKAN_1_3);

		Optimizer.RegisterPerformancePasses();

		bool Success = Optimizer.Run(Result.SPIR_V.ByteCode.data(), Result.SPIR_V.ByteCode.size(), &OptimizedCode);

		if (Success)
			Result.SPIR_V.ByteCode = std::move(OptimizedCode);
	}

	/**************************************************************/
}

glslang::TShader VK_NAMESPACE::ShaderCompiler::MakeGLSLangShader(
	const CompilerEnvironment& in, EShLanguage Stage)
{
	const auto& config = in.GetConfig();

	glslang::TShader shader(Stage);

	shader.setEnvInput(
		glslang::EShSourceGlsl,
		Stage,
		glslang::EShClientVulkan,
		config.GlslVersion);

	shader.setEnvClient(glslang::EShClientVulkan, config.VulkanVersion);
	shader.setEnvTarget(glslang::EShTargetSpv, config.SPV_Version);

	return shader;
}

void VK_NAMESPACE::ShaderCompiler::ReflectShaderMetaData(CompileResult& Result)
{
	auto& ByteCode = Result.SPIR_V.ByteCode;
	Result.MetaData.ShaderType = Result.SPIR_V.Stage;

	spirv_cross::CompilerGLSL compiler(ByteCode);

	if (Result.MetaData.ShaderType == vk::ShaderStageFlagBits::eCompute)
	{
		spirv_cross::SPIREntryPoint entryPoint = compiler.get_entry_point("main", spv::ExecutionModelGLCompute);
		Result.MetaData.WorkGroupSize.x = entryPoint.workgroup_size.x;
		Result.MetaData.WorkGroupSize.y = entryPoint.workgroup_size.y;
		Result.MetaData.WorkGroupSize.z = entryPoint.workgroup_size.z;
	}
}

bool VK_NAMESPACE::ShaderCompiler::ReadAndPreprocess(CompileResult& Result, const ShaderInput& Input)
{
	auto fileError = ReadFile(Input.FilePath);

	if (!fileError)
		return false;

	Result.Error.SrcCode = *fileError;

	auto Includer = mEnvironment.CreateShaderIncluder(Input.FilePath);

	IncludeResult Base;
	Base.Contents = Result.Error.SrcCode;
	Base.HeaderName;

	ResolveIncludeRecursive(Result, Base, Includer, 0);

	if(Result.Error.Type == ErrorType::eNone)
		Result.Error.SrcCode = Base.Contents;

	return true;
}

void VK_NAMESPACE::ShaderCompiler::ResolveIncludeRecursive(CompileResult& Result, IncludeResult& Base,
	std::shared_ptr<ShaderIncluder> Includer, uint32_t RecursionDepth)
{
	Lexer lexer(Base.Contents);
	lexer.SetWhiteSpacesAndDelimiters(" \t", "#<>\"\n");

	Token token = lexer.NextToken();

	while (token.Value.front() && Result.Error.Type == ErrorType::eNone)
	{
		if (token.Value == "#")
		{
			Token nextToken = lexer.NextToken();

			if (!nextToken.Value.front())
				break;

			if (nextToken.Value == "include")
				ParseIncludeDirective(Result, lexer, Base, Includer, RecursionDepth, token);
			else
				token = nextToken;
		}
		else
			token = lexer.NextToken();
	}
}

void VK_NAMESPACE::ShaderCompiler::ParseIncludeDirective(CompileResult& Result, Lexer& lexer, IncludeResult& Base,
	std::shared_ptr<ShaderIncluder> Includer, uint32_t RecursionDepth, Token& token)
{
	std::string lineString = std::to_string(token.LineNumber);
	std::string charOffset = std::to_string(token.CharOffset);

	size_t index = 0;
	std::array<Token, 4> tokens;

	while (index < tokens.size())
	{
		tokens[index] = lexer.NextToken();
		if (tokens[index].Value == "\n")
			break;

		index++;
	}

	if (index != 3)
	{
		Result.Error.Type = ErrorType::ePreprocess;
		Result.Error.Info = "Error: line " + lineString + ", " + charOffset + ": Ill formed #include directive";
		return;
	}

	Token HeaderName = tokens[1];
	std::string IncluderName = Base.HeaderName;

	IncludeResult* Inclusion = nullptr;
	std::filesystem::path includerPath(IncluderName);
	includerPath = includerPath.parent_path();

	if (tokens[0].Value == "<" && tokens[2].Value == ">")
		Inclusion = Includer->IncludeSystem(HeaderName.Value, includerPath.string(), RecursionDepth);
	if (tokens[0].Value == "\"" && tokens[2].Value == "\"")
		Inclusion = Includer->IncludeLocal(HeaderName.Value, includerPath.string(), RecursionDepth);
	else
	{
		Result.Error.Type = ErrorType::ePreprocess;
		Result.Error.Info = "Error: " + lineString + ", " + charOffset + ": Ill formed #include directive";
		return;
	}

	if (!Inclusion)
	{
		Result.Error.Type = ErrorType::ePreprocess;
		Result.Error.Info = "Error: line " + lineString + ", " + charOffset + 
			": Can't open file at " + HeaderName.Value;
		return;
	}

	ResolveIncludeRecursive(Result, *Inclusion, Includer, RecursionDepth + 1);

	size_t InitPos = token.Position;
	size_t FinalPos = tokens[3].Position;

	Base.Contents.erase(InitPos, FinalPos - InitPos);
	Base.Contents.insert(InitPos, Inclusion->Contents);

	lexer = Lexer(Base.Contents);
	lexer.SetWhiteSpacesAndDelimiters(" \t", "#<>\"\n");

	size_t CursorPos = InitPos + Inclusion->Contents.size();

	Includer->ReleaseInclude(Inclusion);

	if (Result.Error.Type != ErrorType::eNone)
		return;

	lexer.SetCursor(CursorPos);
	token = lexer.NextToken();
}

void VK_NAMESPACE::ShaderCompiler::ReflectDescriptorLayouts(CompileResult& Result)
{
	auto& ByteCode = Result.SPIR_V.ByteCode;

	spirv_cross::Compiler Compiler(ByteCode);

	auto ShaderResources = Compiler.get_shader_resources();

	// Function to handle desc sets
	auto FillDescriptor = [&Result, &Compiler]
	(const spirv_cross::SmallVector<spirv_cross::Resource>& Resources, vk::DescriptorType DescType)
	{
		for (const auto& Resource : Resources)
		{
			uint32_t Binding = Compiler.get_decoration(Resource.id, spv::DecorationBinding);
			uint32_t SetIndex = Compiler.get_decoration(Resource.id, spv::DecorationDescriptorSet);
			std::string name = Resource.name;
			const auto& Type = Compiler.get_type(Resource.type_id);

			Result.LayoutData.DescInfos.emplace_back(SetIndex, Binding, name, Type, DescType);
		}
	};

	// Function to handle push constant ranges
	auto FillPushConstants = [&Result, &Compiler]()
	{
		// Get all active push constant buffers
		auto push_constant_resources = Compiler.get_shader_resources().push_constant_buffers;

		// Convert the stage to a string
		std::string stage_name = sCompilerInitializer.GetShaderStageString(Result.SPIR_V.Stage);

		for (const auto& push_constant : push_constant_resources)
		{
			// Extract the type of the push constant
			const auto& push_constant_type = Compiler.get_type(push_constant.base_type_id);

			// Get the size of the push constant block
			size_t push_constant_size = Compiler.get_declared_struct_size(push_constant_type);

			// Extract the range of active push constants within the block
			auto ranges = Compiler.get_active_buffer_ranges(push_constant.id);

			// Sort the ranges into ascending order in index
			std::sort(ranges.begin(), ranges.end(), [](
				const spirv_cross::BufferRange& first, const spirv_cross::BufferRange& second)
			{
				return first.index < second.index;
			});

			// Get push constant name (use type name as a fall back if the name is empty)
			std::string push_constant_name = push_constant.name.empty()
				? Compiler.get_name(push_constant.base_type_id)
				: push_constant.name;

			// Range data for creating pipeline layout
			vk::PushConstantRange& RangeStorage = Result.LayoutData.PushConstantsData.emplace_back();

			RangeStorage.offset = 0;
			RangeStorage.size = static_cast<uint32_t>(push_constant_size);
			RangeStorage.stageFlags = Result.SPIR_V.Stage;

			// This is really just 'std::unordered_map<std::string, vk::PushConstantRange>'
			PushConstantSubrangeInfos& Subranges = Result.LayoutData.PushConstantSubrangeInfos;

			for (const auto& range : ranges)
			{
				vk::PushConstantRange vkRange;
				vkRange.stageFlags = Result.SPIR_V.Stage; // Set stage flags based on the current shader stage
				vkRange.offset = static_cast<uint32_t>(range.offset);
				vkRange.size = static_cast<uint32_t>(range.range);

				// Create a key using the stage name, push constant name (or type name), and range offset
				std::string key = stage_name + "." + push_constant_name + ".Index_" + std::to_string(range.index);

				// Store the vk::PushConstantRange in the map with the concatenated key
				Subranges[key] = vkRange;
			}
		}
	};

	// TODO: Add input attachment descriptor as well!!
	FillDescriptor(ShaderResources.uniform_buffers, vk::DescriptorType::eUniformBuffer);
	FillDescriptor(ShaderResources.storage_buffers, vk::DescriptorType::eStorageBuffer);
	FillDescriptor(ShaderResources.sampled_images, vk::DescriptorType::eCombinedImageSampler);
	FillDescriptor(ShaderResources.separate_samplers, vk::DescriptorType::eSampler);
	FillDescriptor(ShaderResources.separate_images, vk::DescriptorType::eSampledImage);
	FillDescriptor(ShaderResources.storage_images, vk::DescriptorType::eStorageImage);

	// Store the push constants
	FillPushConstants();

	Result.SetLayoutBindingsMap = sCompilerInitializer.GetSetBindings(
		Result.LayoutData.DescInfos, Result.SPIR_V.Stage);
}
