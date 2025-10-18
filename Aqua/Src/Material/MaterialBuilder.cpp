#include "Core/Aqpch.h"
#include "Material/MaterialBuilder.h"
#include "Material/StructPool.h"

std::unordered_map<std::string, std::tuple<uint32_t, uint32_t>> AQUA_NAMESPACE::MaterialBuilder::sGLSLTypeToSize =
{
	{ "int", { 4, 4} }, { "uint", { 4, 4} }, { "float", { 4, 4} },
	{ "vec2", { 8, 8} }, { "ivec2", { 8, 8} }, { "uvec2", { 8, 8} },
	{ "vec3", { 12, 16} }, { "ivec3", { 12, 16} }, { "uvec3", { 12, 16} },
	{ "vec4", { 16, 16} }, { "ivec4", { 16, 16} }, { "uvec4", { 16, 16} },
	{ "mat2", { 16, 16} }, { "umat2", { 16, 16} }, { "imat2", { 16, 16} },
	{ "mat3", { 36, 48} }, { "umat3", { 36, 48} }, { "imat3", { 36, 48} },
	{ "mat4", { 64, 64} }, { "umat4", { 64, 64} }, { "imat4", { 64, 64} },
};

AQUA_NAMESPACE::MaterialInstance AQUA_NAMESPACE::MaterialBuilder::BuildRTInstance(const RTMaterialCreateInfo& createInfo)
{
	vkLib::PreprocessorDirectives directives;
	directives["SHADING_TOLERANCE"] = std::to_string(createInfo.ShadingTolerance);
	directives["EPSILON"] = std::to_string(std::numeric_limits<float>::epsilon());
	directives["POWER_HEURISTICS_EXP"] = std::to_string(createInfo.PowerHeuristics);
	directives["WORKGROUP_SIZE"] = std::to_string(createInfo.WorkGroupSize);
	directives["SHADER_PARS_SET_IDX"] = std::to_string(mMaterialSetBinding.SetIndex);
	directives["SHADER_PARS_BINDING_IDX"] = std::to_string(mMaterialSetBinding.Binding);

	directives["EMPTY_MATERIAL_ID"] = std::to_string(createInfo.EmptyMaterialID);
	directives["SKYBOX_MATERIAL_ID"] = std::to_string(createInfo.SkyboxMaterialID);
	directives["LIGHT_MATERIAL_ID"] = std::to_string(createInfo.LightMaterialID);
	directives["RR_CUTOFF_CONST"] = std::to_string(createInfo.RussianRouletteCutoffConst);

	MAT_NAMESPACE::ShaderParameterSet set;
	uint32_t stride = 0;

	std::string pipelineCode = PostProcessMaterialGraphCode(createInfo.ShaderCode, stride, set, directives, vk::ShaderStageFlagBits::eCompute);
	auto material = mMaterialAsembler.ConstructRayTracingMaterial(pipelineCode, directives);

	MaterialInstance instance{};
	InitializeInstance(instance, *material, set, stride, MAT_NAMESPACE::Platform::ePathTracer);

	return instance;
}

AQUA_NAMESPACE::MaterialInstance AQUA_NAMESPACE::MaterialBuilder::BuildDeferInstance(
	const DeferGFXMaterialCreateInfo& createInfo)
{
	vkLib::PreprocessorDirectives directives;
	directives["SHADING_TOLERANCE"] = std::to_string(createInfo.ShadingTolerance);
	directives["MAX_DEPTH_ARRAY"] = std::to_string(1);
	directives["SHADER_PARS_SET_IDX"] = std::to_string(mMaterialSetBinding.SetIndex);
	directives["SHADER_PARS_BINDING_IDX"] = std::to_string(mMaterialSetBinding.Binding);
	directives["MATH_PI"] = std::to_string(glm::pi<float>());
	directives["TOLERANCE"] = std::to_string(createInfo.ShadingTolerance);

	directives["EMPTY_MATERIAL_ID"] = std::to_string(createInfo.EmptyMaterialID);

	MAT_NAMESPACE::ShaderParameterSet set;
	uint32_t stride = 0;

	std::string pipelineCode = PostProcessMaterialGraphCode(createInfo.ShaderCode, stride, set, directives, vk::ShaderStageFlagBits::eFragment);
	auto material = mMaterialAsembler.ConstructDeferGFXMaterial(pipelineCode, createInfo.GFXConfig, directives);

	MaterialInstance instance{};
	InitializeInstance(instance, *material, set, stride, MAT_NAMESPACE::Platform::eLightingRaster);

	return instance;
}

AQUA_NAMESPACE::MaterialInstance AQUA_NAMESPACE::MaterialBuilder::BuildDeferInstance(const DeferCmpMaterialCreateInfo& createInfo)
{
	vkLib::PreprocessorDirectives directives;
	directives["SHADING_TOLERANCE"] = std::to_string(createInfo.ShadingTolerance);
	directives["MAX_DEPTH_ARRAY"] = std::to_string(1);
	directives["SHADER_PARS_SET_IDX"] = std::to_string(mMaterialSetBinding.SetIndex);
	directives["SHADER_PARS_BINDING_IDX"] = std::to_string(mMaterialSetBinding.Binding);
	directives["MATH_PI"] = std::to_string(glm::pi<float>());
	directives["TOLERANCE"] = std::to_string(createInfo.ShadingTolerance);

	directives["WORK_GROUP_SIZE_X"] = std::to_string(createInfo.WorkGroupSize.x);
	directives["WORK_GROUP_SIZE_Y"] = std::to_string(createInfo.WorkGroupSize.y);

	directives["IMAGE_FORMAT"] = createInfo.ImageFormat;

	directives["EMPTY_MATERIAL_ID"] = std::to_string(createInfo.EmptyMaterialID);

	MAT_NAMESPACE::ShaderParameterSet set;
	uint32_t stride = 0;

	std::string pipelineCode = PostProcessMaterialGraphCode(createInfo.ShaderCode, stride, set, directives, vk::ShaderStageFlagBits::eCompute);
	auto material = mMaterialAsembler.ConstructDeferCmpMaterial(pipelineCode, directives);

	MaterialInstance instance{};
	InitializeInstance(instance, *material, set, stride, MAT_NAMESPACE::Platform::eLightingCompute);

	return instance;
}

void AQUA_NAMESPACE::MaterialBuilder::InitializeInstance(MaterialInstance& instance,
	MAT_NAMESPACE::Material material, 
	MAT_NAMESPACE::ShaderParameterSet& set, 
	uint32_t stride,
	MAT_NAMESPACE::Platform rendererPlatform)
{
	instance.mCoreMaterial = material;

	instance.mInfo = std::make_shared<MaterialInstanceInfo>();
	instance.mInfo->ParameterLocation = mMaterialSetBinding;
	instance.mInfo->RendererType = rendererPlatform;
	instance.mInfo->Resources = {}; // starts at empty
	instance.mInfo->Stride = stride;

	instance.mInfo->ShaderParameters = set;

	instance.mShaderParBuffer = mResourcePool.CreateGenericBuffer(
		vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	instance.mShaderParBuffer.Reserve(64);

	instance.mInstanceID = 0; // Not yet being used by any rendering system

	instance.SetOffset(0);
}

std::vector<std::string> AQUA_NAMESPACE::MaterialBuilder::GetGLSLBasicTypes()
{
	std::vector<std::string> types(sGLSLTypeToSize.size());

	for (const auto& [name, size] : sGLSLTypeToSize)
		types.emplace_back(name);

	return types;
}

void AQUA_NAMESPACE::MaterialBuilder::RetrieveParameterSpecs(MAT_NAMESPACE::ShaderParameterSet& set)
{
	for (auto& [name, parInfo] : set)
	{
		const auto& [size, alignment] = sGLSLTypeToSize.at(parInfo.Type);
		parInfo.TypeSize = size;
		parInfo.Alignment = alignment;
	}
}

std::string AQUA_NAMESPACE::MaterialBuilder::PostProcessMaterialGraphCode(const std::string& shaderCode, uint32_t& stride,
	MAT_NAMESPACE::ShaderParameterSet& set, vkLib::PreprocessorDirectives directives, vk::ShaderStageFlagBits stage)
{
	// remove and resolve all the macros and comments before post processing the material graph code

	vkLib::CompilerEnvironment compilerEnv({});
	compilerEnv.SetPreprocessorDirectives(directives);

	vkLib::ShaderCompiler glslPreprocessor(compilerEnv);
	auto result = glslPreprocessor.PreprocessString(shaderCode, stage);

	auto graphCode = std::move(result.Error.PreprocessedCode);

	// post process the material graph code
	MaterialPostprocessor postProcessor{};
	postProcessor.SetShaderTextView(graphCode);
	postProcessor.SetBasicTypeNames(GetGLSLBasicTypes());
	postProcessor.SetCustomPrefix("__sPCI_CustomParameters[0].__pci_");
	postProcessor.SetShaderModules(mImports);

	auto text1 = postProcessor.ResolveCustomParameters(set);
	postProcessor.SetShaderTextView(*text1);
	auto glslCode = *postProcessor.ResolveImportDirectives();

	DeclareShaderParBuffer(glslCode, stride, set);

	std::string pipelineCode = std::string(mFrontEnd);
	pipelineCode += "\n\n";
	pipelineCode += glslCode;
	pipelineCode += "\n\n";
	pipelineCode += std::string(mBackEnd);


	return pipelineCode;
}

void AQUA_NAMESPACE::MaterialBuilder::DeclareShaderParBuffer(std::string& code, uint32_t& stride,
	MAT_NAMESPACE::ShaderParameterSet& set)
{
	if (set.empty())
		return;

	RetrieveParameterSpecs(set);

	std::string bufferDecl;

	bufferDecl += R"(

layout(std430, set = SHADER_PARS_SET_IDX, binding = SHADER_PARS_BINDING_IDX) readonly buffer __PCI_ShaderParameterSets
{
	__PCI_CustomParameterSet __sPCI_CustomParameters[];
};
)";

	std::string structDecl;

	structDecl = "struct __PCI_CustomParameterSet\n{\n";

	StructPool<uint32_t> structPool{};
	std::vector<MAT_NAMESPACE::ShaderParameter*> pars;

	for (auto& [name, par] : set)
	{
		structPool.PushElement(par.TypeSize, par.Alignment);
		pars.push_back(&par);
	}

	structPool.SetBlockSizeAsMaxAlignment();
	stride = structPool.GetBlockSize();

	auto placementList = *structPool.PackElements();

	std::sort(placementList.begin(), placementList.end(), 
		[](const StructPool<uint32_t>::Placement& first, const StructPool<uint32_t>::Placement& second)
		{
			return first.GlobalOffset < second.GlobalOffset;
		});

	stride *= placementList.back().BlockIndex + 1;

	for (const auto& placement : placementList)
	{
		// grammar of the #glsl struct basic field declaration
		// [#basic_type] [#white_spaces] [#type_name];
		structDecl += "\t" + pars[placement.RefIdx]->Type + " "; // declaring the type
		structDecl += "__pci_" + pars[placement.RefIdx]->Name + ";\n"; // declaring the name of the variable

		// set the offsets...
		pars[placement.RefIdx]->Offset = placement.GlobalOffset;
	}

	structDecl += "};\n";

	code = structDecl + bufferDecl + code;
}
