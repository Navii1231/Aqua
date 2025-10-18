#include "Core/Aqpch.h"
#include "Wavefront/WavefrontEstimator.h"

#include "ShaderCompiler/Lexer.h"
#include "Wavefront/BVHFactory.h"

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::WavefrontEstimator(
	const WavefrontEstimatorCreateInfo& createInfo)
	: mCreateInfo(createInfo)
{
	mResourcePool = mCreateInfo.Context.CreateResourcePool();
	mPipelineBuilder = mCreateInfo.Context.MakePipelineBuilder();

	MAT_NAMESPACE::MaterialAssembler assembler{};
	assembler.SetPipelineBuilder(mPipelineBuilder);

	mMaterialSystem.SetAssembler(assembler);
	mMaterialSystem.SetResourcePool(mCreateInfo.Context.CreateResourcePool());

	RetrieveFrontAndBackEndShaders();
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession AQUA_NAMESPACE::PH_FLUX_NAMESPACE::
	WavefrontEstimator::CreateTraceSession()
{
	TraceSession traceSession{};

	traceSession.mSessionInfo = std::make_shared<SessionInfo>();

	CreateTraceBuffers(*traceSession.mSessionInfo);

	return traceSession;
}

std::expected<::AQUA_NAMESPACE::MaterialInstance, vkLib::CompileError> 
	AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::CreateMaterialInstance(const RTMaterialCreateInfo& createInfo)
{
	/*
	* user defined macro definitions...
	* SHADER_TOLERANCE = 0.001, POWER_HEURISTIC_EXP = 2.0,
	* EMPTY_MATERIAL_ID = -1, SKYBOX_MATERIAL_ID = -2, LIGHT_MATERIAL_ID = -3,
	* RR_CUTOFF_CONST
	*/

	return mMaterialSystem.BuildRTInstance(createInfo);
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::CreateExecutor(const ExecutorCreateInfo& createInfo)
{
	Executor executor{};

	executor.mExecutorInfo = std::make_shared<ExecutionInfo>();
	executor.mExecutorInfo->PipelineResources = CreatePipelines();
	executor.mExecutorInfo->CreateInfo = createInfo;
	executor.mExecutorInfo->CmdAlloc = mCreateInfo.Context.CreateCommandPools()[0];

	// todo; transfer the responsibility to create images to the executor itself
	CreateExecutorBuffers(*executor.mExecutorInfo, createInfo);
	CreateExecutorImages(*executor.mExecutorInfo, createInfo);

	executor.mCmdBufs.reserve(mCreateInfo.Context.GetQueueCount(0));
	executor.mExecutorInfo->Workers.reserve(mCreateInfo.Context.GetQueueCount(0));

	for (size_t i = 0; i < mCreateInfo.Context.GetQueueCount(0); i++)
	{
		executor.mCmdBufs.push_back(executor.mExecutorInfo->CmdAlloc.Allocate());
		executor.mExecutorInfo->Workers.emplace_back(mCreateInfo.Context.FetchWorker(0));
	}

	executor.mCtx = mCreateInfo.Context;

	return executor;
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::ExecutionPipelines AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::CreatePipelines()
{
	RTMaterialCreateInfo inactiveMaterialInfo{};
	inactiveMaterialInfo.PowerHeuristics = 2.0f;
	inactiveMaterialInfo.ShadingTolerance = 0.001f;
	inactiveMaterialInfo.WorkGroupSize = mCreateInfo.IntersectionWorkgroupSize;

	std::string emptyShader = "SampleInfo Evaluate(in Ray ray, in CollisionInfo collisionInfo)"
		"{ SampleInfo sampleInfo; sampleInfo.Weight = 1.0; sampleInfo.Luminance = vec3(0.0);"
		"return sampleInfo; }";

	inactiveMaterialInfo.ShaderCode = emptyShader;

	ExecutionPipelines pipelines;

	pipelines.SortRecorder = std::make_shared<SortRecorder<uint32_t>>(mPipelineBuilder, mResourcePool);
	pipelines.SortRecorder->InvalidateSorterPipeline(mCreateInfo.IntersectionWorkgroupSize);

	//mRayRefs = mPipelineResources.SortRecorder->GetBuffer();

	pipelines.RayGenerator = mPipelineBuilder.BuildComputePipeline<RayGenerationPipeline>(GetRayGenerationShader());
	pipelines.IntersectionPipeline = mPipelineBuilder.BuildComputePipeline<IntersectionPipeline>(GetIntersectionShader());
	pipelines.RaySortPreparer = mPipelineBuilder.BuildComputePipeline<RaySortEpiloguePipeline>(GetRaySortEpilogueShader(RaySortEvent::ePrepare));
	pipelines.RaySortFinisher = mPipelineBuilder.BuildComputePipeline<RaySortEpiloguePipeline>(GetRaySortEpilogueShader(RaySortEvent::eFinish));
	pipelines.RayRefCounter = mPipelineBuilder.BuildComputePipeline<RayRefCounterPipeline>(GetRayRefCounterShader());
	pipelines.PrefixSummer = mPipelineBuilder.BuildComputePipeline<PrefixSumPipeline>(GetPrefixSumShader());
	pipelines.InactiveRayShader = *CreateMaterialInstance(inactiveMaterialInfo);
	pipelines.LuminanceMean = mPipelineBuilder.BuildComputePipeline<LuminanceMeanPipeline>(GetLuminanceMeanShader());
	pipelines.PostProcessor = mPipelineBuilder.BuildComputePipeline<PostProcessImagePipeline>(GetPostProcessImageShader());

	return pipelines;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::CreateTraceBuffers(SessionInfo& session)
{
	vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eStorageBuffer;
	vk::MemoryPropertyFlags memProps = vk::MemoryPropertyFlagBits::eHostCoherent;

	session.SharedBuffers.Vertices = mResourcePool.CreateBuffer<glm::vec4>(usage, memProps);
	session.SharedBuffers.Faces = mResourcePool.CreateBuffer<Face>(usage, memProps);
	session.SharedBuffers.TexCoords = mResourcePool.CreateBuffer<glm::vec2>(usage, memProps);
	session.SharedBuffers.Normals = mResourcePool.CreateBuffer<glm::vec4>(usage, memProps);
	session.SharedBuffers.Nodes = mResourcePool.CreateBuffer<Node>(usage, memProps);

	session.MeshInfos = mResourcePool.CreateBuffer<MeshInfo>(usage, memProps);
	session.LightInfos = mResourcePool.CreateBuffer<LightInfo>(usage, memProps);
	session.LightPropsInfos = mResourcePool.CreateBuffer<LightProperties>(usage, memProps);

	memProps = vk::MemoryPropertyFlagBits::eDeviceLocal;

	session.LocalBuffers.Vertices = mResourcePool.CreateBuffer<glm::vec4>(usage, memProps);
	session.LocalBuffers.Faces = mResourcePool.CreateBuffer<Face>(usage, memProps);
	session.LocalBuffers.TexCoords = mResourcePool.CreateBuffer<glm::vec2>(usage, memProps);
	session.LocalBuffers.Normals = mResourcePool.CreateBuffer<glm::vec4>(usage, memProps);
	session.LocalBuffers.Nodes = mResourcePool.CreateBuffer<Node>(usage, memProps);

	// SceneInfo and physical camera buffer is a uniform and should be host coherent...
	usage = vk::BufferUsageFlagBits::eUniformBuffer;
	memProps = vk::MemoryPropertyFlagBits::eHostCoherent;

	// Intersection stage...
	session.CameraSpecsBuffer = mResourcePool.CreateBuffer<PhysicalCamera>(usage, memProps);
	session.ShaderConstData = mResourcePool.CreateBuffer<ShaderData>(usage, memProps);

	session.CameraSpecsBuffer.Resize(1);
	session.ShaderConstData.Resize(1);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::CreateExecutorBuffers(
	ExecutionInfo& executionInfo, const ExecutorCreateInfo& executorInfo)
{
	vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eStorageBuffer | executorInfo.BufferUsage;
	vk::MemoryPropertyFlags memProps = executorInfo.MemoryProps;

	executionInfo.PipelineResources.SortRecorder->ResizeBuffer(2 * executorInfo.TileSize.x * executorInfo.TileSize.y);
	executionInfo.RayRefs = executionInfo.PipelineResources.SortRecorder->GetBuffer();

	executionInfo.Rays = mResourcePool.CreateBuffer<Ray>(usage, memProps);
	executionInfo.RayInfos = mResourcePool.CreateBuffer<RayInfo>(usage, memProps);
	executionInfo.CollisionInfos = mResourcePool.CreateBuffer<CollisionInfo>(usage, memProps);

	executionInfo.RefCounts = mResourcePool.CreateBuffer<uint32_t>(usage, memProps);

	uint32_t RayCount = executorInfo.TargetResolution.x * executorInfo.TargetResolution.y;

	executionInfo.Rays.Resize(2 * RayCount);
	executionInfo.RayInfos.Resize(2 * RayCount);
	executionInfo.CollisionInfos.Resize(2 * RayCount);

	// can't be configured by the user
	usage = vk::BufferUsageFlagBits::eUniformBuffer;
	memProps = vk::MemoryPropertyFlagBits::eHostCoherent;

	executionInfo.Scene = mResourcePool.CreateBuffer<WavefrontSceneInfo>(usage, memProps, 1);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::CreateExecutorImages(
	ExecutionInfo& executionInfo, const ExecutorCreateInfo& executorInfo)
{
	vkLib::ImageCreateInfo imageInfo{};
	imageInfo.Extent = vk::Extent3D(executorInfo.TileSize.x, executorInfo.TileSize.y, 1);
	imageInfo.Format = vk::Format::eR32G32B32A32Sfloat;
	imageInfo.MemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;
	imageInfo.Type = vk::ImageType::e2D;
	imageInfo.Usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;

	executionInfo.Target.PixelMean = mResourcePool.CreateImage(imageInfo);
	executionInfo.Target.PixelVariance = mResourcePool.CreateImage(imageInfo);

	imageInfo.Format = vk::Format::eR8G8B8A8Unorm;
	executionInfo.Target.Presentable = mResourcePool.CreateImage(imageInfo);

	executionInfo.Target.ImageResolution = executorInfo.TargetResolution;

	executionInfo.Target.PixelMean.TransitionLayout(
		vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTopOfPipe);

	executionInfo.Target.PixelVariance.TransitionLayout(
		vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTopOfPipe);

	executionInfo.Target.Presentable.TransitionLayout(
		vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTopOfPipe);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::AddText(std::string& text, const std::string& filepath)
{
	text += *vkLib::ReadFile(filepath) + "\n\n";
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::RetrieveFrontAndBackEndShaders()
{
	// TODO: Retrieve the vulkan version from the vkLib library...
	mShaderFrontEnd = "#version 440\n\n";

	// All the front shaders and custom libraries...
	AddText(mShaderFrontEnd, GetShaderDirectory() + "Wavefront/Common.glsl");
	AddText(mShaderFrontEnd, GetShaderDirectory() + "BSDFs/CommonBSDF.glsl");
	AddText(mShaderFrontEnd, GetShaderDirectory() + "BSDFs/BSDF_Samplers.glsl");
	AddText(mShaderFrontEnd, GetShaderDirectory() + "MaterialShaders/ShaderFrontEnd.glsl");
	AddText(mShaderFrontEnd, GetShaderDirectory() + "BSDFs/Utils.glsl");

	/* TODO: This is temporary, should be dealt by an import system */

#if 0
	AddText(mShaderFrontEnd, "Shaders/BSDFs/DiffuseBSDF.glsl");
	AddText(mShaderFrontEnd, "Shaders/BSDFs/GlossyBSDF.glsl");
	AddText(mShaderFrontEnd, "Shaders/BSDFs/RefractionBSDF.glsl");
	AddText(mShaderFrontEnd, "Shaders/BSDFs/CookTorranceBSDF.glsl");
	AddText(mShaderFrontEnd, "Shaders/BSDFs/GlassBSDF.glsl");
#else
	AddText(mImportToShaders["DiffuseBSDF"], GetShaderDirectory() + "BSDFs/DiffuseBSDF.glsl");
	AddText(mImportToShaders["GlossyBSDF"], GetShaderDirectory() + "BSDFs/GlossyBSDF.glsl");
	AddText(mImportToShaders["RefractionBSDF"], GetShaderDirectory() + "BSDFs/RefractionBSDF.glsl");
	AddText(mImportToShaders["CookTorranceBSDF"], GetShaderDirectory() + "BSDFs/CookTorranceBSDF.glsl");
	AddText(mImportToShaders["GlassBSDF"], GetShaderDirectory() + "BSDFs/GlassBSDF.glsl");
#endif

	/****************************************************************/

	// Back end of the pipelines handling luminance calculations
	AddText(mShaderBackEnd, GetShaderDirectory() + "MaterialShaders/ShaderBackEnd.glsl");

	mMaterialSystem.SetFrontEndView(mShaderFrontEnd);
	mMaterialSystem.SetBackEndView(mShaderBackEnd);

	mMaterialSystem.SetImports(mImportToShaders);
}

void DispatchErrorMessage(AQUA_NAMESPACE::PH_FLUX_NAMESPACE::MaterialShaderError error)
{
	std::string errorMessage;

	switch (error.State)
	{
		case AQUA_NAMESPACE::PH_FLUX_NAMESPACE::MaterialPreprocessState::eSuccess:
			return;
		case AQUA_NAMESPACE::PH_FLUX_NAMESPACE::MaterialPreprocessState::eInvalidSyntax:
			errorMessage = "Invalid import syntax: " + error.Info;
			break;
		case AQUA_NAMESPACE::PH_FLUX_NAMESPACE::MaterialPreprocessState::eShaderNotFound:
			errorMessage = "Could not import the shader: " + error.Info;
			break;
		default:
			errorMessage = "Unknown shader error!";
			break;
	}

	_STL_ASSERT(false, errorMessage.c_str());
}

vkLib::PShader AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::GetRayGenerationShader()
{
	vkLib::OptimizerFlag optimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	optimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader{};

	shader.AddMacro("WORKGROUP_SIZE", std::to_string(mCreateInfo.RayGenWorkgroupSize.x));
	shader.SetFilepath("eCompute", GetShaderDirectory() + "Wavefront/RayGeneration.comp", optimizerFlag);

	auto Errors = shader.CompileShaders();

	CompileErrorChecker checker("Logging/ShaderFails/Shader.glsl");
	checker.AssertOnError(Errors);

	return shader;
}

vkLib::PShader AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::GetIntersectionShader()
{
	vkLib::OptimizerFlag optimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	optimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader;

	shader.AddMacro("WORKGROUP_SIZE", std::to_string(mCreateInfo.IntersectionWorkgroupSize));
	shader.AddMacro("TOLERANCE", std::to_string(mCreateInfo.Tolerance));
	shader.AddMacro("MAX_DIS", std::to_string(FLT_MAX));
	shader.AddMacro("FLT_MAX", std::to_string(FLT_MAX));

	shader.SetFilepath("eCompute", GetShaderDirectory() + "Wavefront/Intersection.glsl", OPTIMIZE_INTERSECTION == 1 ?
		vkLib::OptimizerFlag::eO3 : optimizerFlag);

	auto Errors = shader.CompileShaders();

	CompileErrorChecker checker("../vkEngineTester/Logging/ShaderFails/Shader.glsl");

	auto ErrorInfos = checker.GetErrors(Errors);
	checker.AssertOnError(ErrorInfos);

	return shader;
}

vkLib::PShader AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::GetRaySortEpilogueShader(RaySortEvent sortEvent)
{
	vkLib::OptimizerFlag optimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	optimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader;

	shader.AddMacro("WORKGROUP_SIZE", std::to_string(mCreateInfo.IntersectionWorkgroupSize));

	std::string shaderPath;

	switch (sortEvent)
	{
		case RaySortEvent::ePrepare:
			shaderPath = GetShaderDirectory() + "Wavefront/PrepareRaySort.glsl";
			break;
		case RaySortEvent::eFinish:
			shaderPath = GetShaderDirectory() + "Wavefront/FinishRaySort.glsl";
			break;
		default:
			break;
	}

	shader.SetFilepath("eCompute", shaderPath, optimizerFlag);

	auto Errors = shader.CompileShaders();

	CompileErrorChecker checker("../vkEngineTester/Logging/ShaderFails/Shader.glsl");

	auto ErrorInfos = checker.GetErrors(Errors);
	checker.AssertOnError(ErrorInfos);

	return shader;
}

vkLib::PShader AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::GetRayRefCounterShader()
{
	vkLib::OptimizerFlag optimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	optimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader;

	shader.AddMacro("WORKGROUP_SIZE", std::to_string(mCreateInfo.IntersectionWorkgroupSize));
	shader.AddMacro("PRIMITIVE_TYPE", "uint");

	shader.SetFilepath("eCompute", GetShaderDirectory() + "Utils/CountElements.glsl", optimizerFlag);

	auto Errors = shader.CompileShaders();

	CompileErrorChecker checker("../vkEngineTester/Logging/ShaderFails/Shader.glsl");

	auto ErrorInfos = checker.GetErrors(Errors);
	checker.AssertOnError(ErrorInfos);

	return shader;
}

vkLib::PShader AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::GetPrefixSumShader()
{
	vkLib::OptimizerFlag optimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	optimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader;

	shader.AddMacro("WORKGROUP_SIZE", std::to_string(mCreateInfo.IntersectionWorkgroupSize));
	shader.SetFilepath("eCompute", GetShaderDirectory() + "Utils/PrefixSum.glsl", optimizerFlag);
	
	auto Errors = shader.CompileShaders();

	CompileErrorChecker checker("Logging/ShaderFails/Shader.glsl");

	auto ErrorInfos = checker.GetErrors(Errors);
	checker.AssertOnError(ErrorInfos);

	return shader;
}

vkLib::PShader AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::GetLuminanceMeanShader()
{
	vkLib::OptimizerFlag optimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	optimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader;

	shader.AddMacro("WORKGROUP_SIZE", std::to_string(mCreateInfo.IntersectionWorkgroupSize));
	shader.SetFilepath("eCompute", GetShaderDirectory() + "Wavefront/LuminanceMean.glsl", optimizerFlag);
	
	auto Errors = shader.CompileShaders();

	CompileErrorChecker checker("Logging/ShaderFails/Shader.glsl");

	auto ErrorInfos = checker.GetErrors(Errors);
	checker.AssertOnError(ErrorInfos);

	return shader;
}

vkLib::PShader AQUA_NAMESPACE::PH_FLUX_NAMESPACE::WavefrontEstimator::GetPostProcessImageShader()
{
	vkLib::OptimizerFlag optimizerFlag = vkLib::OptimizerFlag::eO3;

#if _DEBUG
	optimizerFlag = vkLib::OptimizerFlag::eNone;
#endif

	vkLib::PShader shader;

	shader.AddMacro("WORKGROUP_SIZE_X", std::to_string(16));
	shader.AddMacro("WORKGROUP_SIZE_Y", std::to_string(16));

	shader.AddMacro("APPLY_TONE_MAP", std::to_string(static_cast<uint32_t>(PostProcessFlagBits::eToneMap)));

	shader.AddMacro("APPLY_GAMMA_CORRECTION",
		std::to_string(static_cast<uint32_t>(PostProcessFlagBits::eGammaCorrection)));

	shader.AddMacro("APPLY_GAMMA_CORRECTION_INV",
		std::to_string(static_cast<uint32_t>(PostProcessFlagBits::eGammaCorrectionInv)));

	shader.SetFilepath("eCompute", GetShaderDirectory() + "Wavefront/PostProcessImage.glsl", optimizerFlag);

	auto Errors = shader.CompileShaders();

	CompileErrorChecker checker("../vkEngineTester/Logging/ShaderFails/Shader.glsl");

	auto ErrorInfos = checker.GetErrors(Errors);
	checker.AssertOnError(ErrorInfos);

	return shader;
}
