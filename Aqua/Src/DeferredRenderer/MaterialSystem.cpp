#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/MaterialSystem.h"

#include "DeferredRenderer/Pipelines/DeferredPipeline.h"
#include "DeferredRenderer/Pipelines/HyperSurface.h"
#include "DeferredRenderer/Pipelines/Wireframe.h"

#include "Material/MaterialInstance.h"
#include "Material/MaterialBuilder.h"

#include "DeferredRenderer/Renderable/RenderTargetFactory.h"

#include "Utils/CompilerErrorChecker.h"

AQUA_BEGIN

using MaterialCache = std::unordered_map<std::string, SharedRef<MaterialTemplate>>;

struct MaterialSystemConfig
{
	MaterialCache mCache;

	HyperSurfacePipeline mPointPipeline;
	HyperSurfacePipeline mLinePipeline;
	WireframePipeline mWireframePipeline;

	vkLib::RenderTargetContext mHyperSurfCtx;
	vkLib::RenderTargetContext mPBRCtx;
	vkLib::RenderTargetContext mDiffuseCtx;
	vkLib::RenderTargetContext mGlossyCtx;
	vkLib::RenderTargetContext mWireframeCtx;

	MaterialBuilder mBuilder;
	vkLib::PipelineBuilder mPipelineBuilder;

	// relative path to application
	std::filesystem::path mShaderDirectory;

	std::string mFrontEnd;
	std::string mBackEnd;

	std::string mPBRShaderString;
	std::string mGlossyShaderString;
	std::string mDiffuseShaderString;

	vkLib::PShader mHyperSurfaceShader;
	vkLib::PShader mWireframeShader;

	vkLib::Context mCtx;
};

AQUA_END

AQUA_NAMESPACE::MaterialSystem::MaterialSystem()
{
	mConfig = MakeRef<MaterialSystemConfig>();

	SetupDeferShaderStrings();
}

AQUA_NAMESPACE::MaterialSystem::MaterialSystem(vkLib::Context ctx, const std::filesystem::path& shaderDir)
{
	mConfig = MakeRef<MaterialSystemConfig>();

	SetupDeferShaderStrings();

	SetCtx(ctx);
	SetShaderDirectory(shaderDir);
}

void AQUA_NAMESPACE::MaterialSystem::SetShaderDirectory(const std::filesystem::path& dir)
{
	mConfig->mShaderDirectory = dir;

	MAT_NAMESPACE::MaterialAssembler assembler{};
	assembler.SetPipelineBuilder(mConfig->mPipelineBuilder);

	auto& builder = mConfig->mBuilder;
	auto& directory = mConfig->mShaderDirectory;

	std::string frontEndDefs = *vkLib::ReadFile((directory / "FrontEnd.glsl").string());
	std::string bsdfUtility = *vkLib::ReadFile((directory / "CommonBSDFs.glsl").string());
	mConfig->mBackEnd = *vkLib::ReadFile((directory / "BackEnd.glsl").string());

	mConfig->mFrontEnd = frontEndDefs + "\n";
	mConfig->mFrontEnd += bsdfUtility;

	builder.SetAssembler(assembler);
	builder.SetResourcePool(mConfig->mCtx.CreateResourcePool());

	builder.SetFrontEndView(mConfig->mFrontEnd);
	builder.SetBackEndView(mConfig->mBackEnd);

	InsertImports();

	CompileHyperSurfShader();
	CompileWireframeShader();
}

void AQUA_NAMESPACE::MaterialSystem::SetHyperSurfRenderProperties(vkLib::RenderTargetContext rCtx)
{
	mConfig->mHyperSurfCtx = rCtx;
	BuildHyperSurfPipelines();
}

void AQUA_NAMESPACE::MaterialSystem::SetPBRRenderProperties(vkLib::RenderTargetContext rCtx)
{
	mConfig->mPBRCtx = rCtx;
	BuildPBRPipeline();
}

void AQUA_NAMESPACE::MaterialSystem::SetDiffuseRenderProperties(vkLib::RenderTargetContext rCtx)
{
	mConfig->mDiffuseCtx = rCtx;
	BuildDiffusePipeline();
}

void AQUA_NAMESPACE::MaterialSystem::SetGlossyRenderProperties(vkLib::RenderTargetContext rCtx)
{
	mConfig->mGlossyCtx = rCtx;
	BuildGlossyPipeline();
}

void AQUA_NAMESPACE::MaterialSystem::SetWireframeRenderProperties(vkLib::RenderTargetContext rCtx)
{
	mConfig->mWireframeCtx = rCtx;
	BuildWireframePipeline();
}

void AQUA_NAMESPACE::MaterialSystem::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;
	mConfig->mPipelineBuilder = ctx.MakePipelineBuilder();
}

AQUA_NAMESPACE::MaterialInstance AQUA_NAMESPACE::MaterialSystem::BuildInstance(
	const std::string& name, const DeferGFXMaterialCreateInfo& createInfo) const
{
	auto instance = mConfig->mBuilder.BuildDeferInstance(createInfo);
	auto temp = CreateTemplate(name, createInfo, instance);

	mConfig->mCache[name] = temp;

	return instance;
}

AQUA_NAMESPACE::MaterialInstance AQUA_NAMESPACE::MaterialSystem::BuildInstance(const std::string& name, const DeferCmpMaterialCreateInfo& createInfo) const
{
	auto instance = mConfig->mBuilder.BuildDeferInstance(createInfo);
	auto temp = CreateTemplate(name, createInfo, instance);

	mConfig->mCache[name] = temp;

	return instance;
}

void AQUA_NAMESPACE::MaterialSystem::InitInstance(MaterialTemplate& temp, MaterialInstance& instance, MAT_NAMESPACE::Platform rendererPlatform) const
{
	mConfig->mBuilder.InitializeInstance(instance, temp.MatOp, temp.ParameterSet, temp.ParStride, rendererPlatform);
}

AQUA_NAMESPACE::SharedRef<AQUA_NAMESPACE::MaterialTemplate> AQUA_NAMESPACE::MaterialSystem::
	CreateTemplate(const std::string& name, const DeferGFXMaterialCreateInfo& createInfo, const MaterialInstance& clone) const
{
	SharedRef<MaterialTemplate> temp = MakeRef<MaterialTemplate>();

	temp->GFXCreateInfo = createInfo;
	temp->MatOp = clone.GetMaterial();
	temp->Name = name;
	temp->ParameterSet = clone.GetInfo()->ShaderParameters;

	return temp;
}

AQUA_NAMESPACE::SharedRef<AQUA_NAMESPACE::MaterialTemplate> AQUA_NAMESPACE::MaterialSystem::CreateTemplate(const std::string& name, const DeferCmpMaterialCreateInfo& createInfo, const MaterialInstance& clone) const
{
	SharedRef<MaterialTemplate> temp = MakeRef<MaterialTemplate>();

	temp->CmpCreateInfo = createInfo;
	temp->MatOp = clone.GetMaterial();
	temp->Name = name;
	temp->ParameterSet = clone.GetInfo()->ShaderParameters;

	return temp;
}

void AQUA_NAMESPACE::MaterialSystem::BuildHyperSurfPipelines()
{
	mConfig->mPointPipeline = mConfig->mPipelineBuilder.BuildGraphicsPipeline<HyperSurfacePipeline>(
		HyperSurfaceType::ePoint, glm::uvec2(1024, 1024), mConfig->mHyperSurfaceShader, mConfig->mHyperSurfCtx);

	mConfig->mLinePipeline = mConfig->mPipelineBuilder.BuildGraphicsPipeline<HyperSurfacePipeline>(
		HyperSurfaceType::eLine, glm::uvec2(1024, 1024), mConfig->mHyperSurfaceShader, mConfig->mHyperSurfCtx);

	auto& cache = mConfig->mCache;

	cache[TEMPLATE_POINT] = MakeRef<MaterialTemplate>();
	cache[TEMPLATE_LINE] = MakeRef<MaterialTemplate>();

	cache[TEMPLATE_POINT]->Name = TEMPLATE_POINT;
	cache[TEMPLATE_POINT]->ParameterSet = {};
	cache[TEMPLATE_POINT]->MatOp.GFX = MakeRef(mConfig->mPointPipeline);
	cache[TEMPLATE_POINT]->MatOp.States.Type = EXEC_NAMESPACE::OpType::eGraphics;
	cache[TEMPLATE_POINT]->RendererPlatform = MAT_NAMESPACE::Platform::eLightingRaster;

	cache[TEMPLATE_LINE]->Name = TEMPLATE_LINE;
	cache[TEMPLATE_LINE]->ParameterSet = {};
	cache[TEMPLATE_LINE]->MatOp.GFX = MakeRef(mConfig->mLinePipeline);
	cache[TEMPLATE_LINE]->MatOp.States.Type = EXEC_NAMESPACE::OpType::eGraphics;
	cache[TEMPLATE_LINE]->RendererPlatform = MAT_NAMESPACE::Platform::eLightingRaster;
}

void AQUA_NAMESPACE::MaterialSystem::BuildWireframePipeline()
{
	mConfig->mWireframePipeline = mConfig->mPipelineBuilder.BuildGraphicsPipeline<WireframePipeline>
		(mConfig->mWireframeShader, glm::vec2(1024, 1024), mConfig->mWireframeCtx);

	auto& cache = mConfig->mCache;

	cache[TEMPLATE_Wireframe] = MakeRef<MaterialTemplate>();

	cache[TEMPLATE_Wireframe]->Name = TEMPLATE_Wireframe;
	cache[TEMPLATE_Wireframe]->ParameterSet = {};
	cache[TEMPLATE_Wireframe]->MatOp.GFX = MakeRef(mConfig->mWireframePipeline);
	cache[TEMPLATE_Wireframe]->MatOp.States.Type = EXEC_NAMESPACE::OpType::eGraphics;
	cache[TEMPLATE_Wireframe]->RendererPlatform = MAT_NAMESPACE::Platform::eLightingRaster;
}

void AQUA_NAMESPACE::MaterialSystem::BuildPBRPipeline()
{
	DeferCmpMaterialCreateInfo createInfo{};
	createInfo.ShadingTolerance = 0.01f;
	createInfo.ShaderCode = mConfig->mPBRShaderString;

	// todo: provided by the user
	createInfo.ImageFormat = "RGBA32F";
	createInfo.WorkGroupSize = { 16, 16 };

	MaterialInstance instance = mConfig->mBuilder.BuildDeferInstance(createInfo);

	mConfig->mCache[TEMPLATE_PBR] = MakeRef<MaterialTemplate>();

	mConfig->mCache[TEMPLATE_PBR]->CmpCreateInfo = createInfo;
	mConfig->mCache[TEMPLATE_PBR]->MatOp = instance.GetMaterial();
	mConfig->mCache[TEMPLATE_PBR]->MatOp.States.Type = EXEC_NAMESPACE::OpType::eCompute;
	mConfig->mCache[TEMPLATE_PBR]->Name = TEMPLATE_PBR;
	mConfig->mCache[TEMPLATE_PBR]->ParameterSet = instance.GetInfo()->ShaderParameters;
	mConfig->mCache[TEMPLATE_PBR]->ParStride = instance.GetInfo()->Stride;
	mConfig->mCache[TEMPLATE_PBR]->RendererPlatform = instance.GetRendererPlatform();

	_STL_VERIFY(*instance.SetShaderParameter("roughness", 0.15f), "couldn't initiate the shader parameters");
	_STL_VERIFY(*instance.SetShaderParameter("metallic", 0.1f), "couldn't initiate the shader parameters");
	_STL_VERIFY(*instance.SetShaderParameter("base_color", glm::vec3(0.6f)), "couldn't initiate the shader parameters");
	_STL_VERIFY(*instance.SetShaderParameter("refract_idx", 7.5f), "couldn't initiate the shader parameters");
}

void AQUA_NAMESPACE::MaterialSystem::BuildDiffusePipeline()
{
	DeferCmpMaterialCreateInfo createInfo{};
	createInfo.ShadingTolerance = 0.01f;
	createInfo.ShaderCode = mConfig->mDiffuseShaderString;

	// todo: provided by the user
	createInfo.ImageFormat = "RGBA32F";
	createInfo.WorkGroupSize = { 16, 16 };

	MaterialInstance instance = mConfig->mBuilder.BuildDeferInstance(createInfo);

	mConfig->mCache[TEMPLATE_Diffuse] = MakeRef<MaterialTemplate>();

	mConfig->mCache[TEMPLATE_Diffuse]->CmpCreateInfo = createInfo;
	mConfig->mCache[TEMPLATE_Diffuse]->MatOp = instance.GetMaterial();
	mConfig->mCache[TEMPLATE_Diffuse]->MatOp.States.Type = EXEC_NAMESPACE::OpType::eCompute;
	mConfig->mCache[TEMPLATE_Diffuse]->Name = TEMPLATE_Diffuse;
	mConfig->mCache[TEMPLATE_Diffuse]->ParameterSet = instance.GetInfo()->ShaderParameters;
	mConfig->mCache[TEMPLATE_Diffuse]->ParStride = instance.GetInfo()->Stride;
	mConfig->mCache[TEMPLATE_Diffuse]->RendererPlatform = instance.GetRendererPlatform();

	_STL_VERIFY(*instance.SetShaderParameter("base_color", glm::vec3(0.6f)), "couldn't initiate the shader parameters");
}

void AQUA_NAMESPACE::MaterialSystem::BuildGlossyPipeline()
{
	DeferCmpMaterialCreateInfo createInfo{};
	createInfo.ShadingTolerance = 0.01f;
	createInfo.ShaderCode = mConfig->mGlossyShaderString;

	// todo: provided by the user
	createInfo.ImageFormat = "RGBA32F";
	createInfo.WorkGroupSize = { 16, 16 };

	MaterialInstance instance = mConfig->mBuilder.BuildDeferInstance(createInfo);

	mConfig->mCache[TEMPLATE_Glossy] = MakeRef<MaterialTemplate>();

	mConfig->mCache[TEMPLATE_Glossy]->CmpCreateInfo = createInfo;
	mConfig->mCache[TEMPLATE_Glossy]->MatOp = instance.GetMaterial();
	mConfig->mCache[TEMPLATE_Glossy]->MatOp.States.Type = EXEC_NAMESPACE::OpType::eCompute;
	mConfig->mCache[TEMPLATE_Glossy]->Name = TEMPLATE_Glossy;
	mConfig->mCache[TEMPLATE_Glossy]->ParameterSet = instance.GetInfo()->ShaderParameters;
	mConfig->mCache[TEMPLATE_Glossy]->ParStride = instance.GetInfo()->Stride;
	mConfig->mCache[TEMPLATE_Glossy]->RendererPlatform = instance.GetRendererPlatform();

	_STL_VERIFY(*instance.SetShaderParameter("base_color", glm::vec3(0.6f)), "couldn't initiate the shader parameters");
	_STL_VERIFY(*instance.SetShaderParameter("roughness", 0.15f), "couldn't initiate the shader parameters");
}

void AQUA_NAMESPACE::MaterialSystem::CompileHyperSurfShader()
{
	vkLib::PShader& shader = mConfig->mHyperSurfaceShader;
	shader.SetFilepath("eVertex", (mConfig->mShaderDirectory / "HyperSurface.vert").string());
	shader.SetFilepath("eFragment", (mConfig->mShaderDirectory / "HyperSurface.frag").string());

	shader.CompileShaders();
}

void AQUA_NAMESPACE::MaterialSystem::CompileWireframeShader()
{
	vkLib::PShader& shader = mConfig->mWireframeShader;
	shader.SetFilepath("eVertex", (mConfig->mShaderDirectory / "Wireframe.vert").string());
	shader.SetFilepath("eFragment", (mConfig->mShaderDirectory / "Wireframe.frag").string());

	shader.CompileShaders();
}

void AQUA_NAMESPACE::MaterialSystem::InsertImports()
{
	auto& builder = mConfig->mBuilder;
	auto& directory = mConfig->mShaderDirectory;

	MAT_NAMESPACE::ShaderImportMap importMap{};

	importMap["CookTorranceBSDF"]  = *vkLib::ReadFile((directory / "CookTorranceBSDF.glsl").string());
	importMap["DiffuseBSDF"]  = *vkLib::ReadFile((directory / "DiffuseBSDF.glsl").string());
	importMap["GlossyBSDF"]  = *vkLib::ReadFile((directory / "GlossyBSDF.glsl").string());

	builder.SetImports(importMap);
}

void AQUA_NAMESPACE::MaterialSystem::SetupDeferShaderStrings()
{
	mConfig->mPBRShaderString = R"(
	import CookTorranceBSDF
	
	vec3 Evaluate(BSDFInput bsdfInput)
	{
		CookTorranceBSDFInput cookTorrInput;
		cookTorrInput.ViewDir = bsdfInput.ViewDir;
		cookTorrInput.LightDir = bsdfInput.LightDir;
		cookTorrInput.Normal = bsdfInput.Normal;
	
		cookTorrInput.BaseColor = vec3.base_color;
		cookTorrInput.Metallic = float.metallic;
		cookTorrInput.Roughness = float.roughness;
		cookTorrInput.RefractiveIndex = float.refract_idx;
		cookTorrInput.TransmissionWeight = 0.0;
	
		return CookTorranceBRDF(cookTorrInput);
	})";

	mConfig->mGlossyShaderString = R"(
	import GlossyBSDF
	
	vec3 Evaluate(BSDFInput bsdfInput)
	{
		GlossyBSDFInput cookTorrInput;
		glossyInput.ViewDir = bsdfInput.ViewDir;
		glossyInput.LightDir = bsdfInput.LightDir;
		glossyInput.Normal = bsdfInput.Normal;
	
		glossyInput.BaseColor = vec3.base_color;
		glossyInput.Roughness = float.roughness;
	
		return GlossyBSDF(glossyInput);
	})";

	mConfig->mDiffuseShaderString = R"(
	import DiffuseBSDF
	
	vec3 Evaluate(BSDFInput bsdfInput)
	{
		DiffuseBSDFInput cookTorrInput;
		diffuseInput.ViewDir = bsdfInput.ViewDir;
		diffuseInput.LightDir = bsdfInput.LightDir;
		diffuseInput.Normal = bsdfInput.Normal;
	
		diffuseInput.BaseColor = vec3.base_color;
	
		return DiffuseBSDF(diffuseInput);
	})";
}

AQUA_NAMESPACE::SharedRef<AQUA_NAMESPACE::MaterialTemplate> AQUA_NAMESPACE::
	MaterialSystem::FindTemplate(const std::string& name) const
{
	if (mConfig->mCache.find(name) == mConfig->mCache.end())
		return {};

	return mConfig->mCache[name];
}

std::expected<AQUA_NAMESPACE::MaterialInstance, bool> AQUA_NAMESPACE::
	MaterialSystem::operator[](const std::string& name) const
{
	auto temp = FindTemplate(name);

	if (!temp)
		return std::unexpected(false);

	MaterialInstance instance{};
	InitInstance(*temp, instance, temp->RendererPlatform);

	return instance;
}
