#include "Core/Aqpch.h"
#include "Wavefront/Executor.h"

#define MAX_UINT32 (static_cast<uint32_t>(~0))

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::~Executor()
{
	for (auto cmdBuf : mCmdBufs)
		mExecutorInfo->CmdAlloc.Free(cmdBuf);
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceResult AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::Trace()
{
	_STL_ASSERT(!mDebugMode, "Can't trace in debug mode, use step function or disable debug mode");

	Reset();
	TraceResult traceStatus = StepImpl();

	while (traceStatus == TraceResult::ePending || traceStatus == TraceResult::eTraversing)
		traceStatus = StepImpl();

	return traceStatus;
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceResult AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::Step()
{
	_STL_ASSERT(mDebugMode, "Can't step in debug mode, use trace function or enable debug mode");

	return StepImpl();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::SetDebugMode(bool enable)
{
	if (mDebugMode == enable)
		return;

	mDebugMode = enable;

	if (mDebugMode)
		SeparateGraphs();
	else
		JoinGraphs();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::InvalidateMaterialResources()
{
	InvalidateMaterialData();
	UpdateMaterialDescriptors();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::Reset()
{
	mExecutionBlock = {};
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ConstructExecutionGraphs(uint32_t depth)
{
	mMaxBounce = depth;

	EXEC_NAMESPACE::GraphBuilder rayGenBuilder;
	EXEC_NAMESPACE::GraphBuilder traceStepBuilder;
	EXEC_NAMESPACE::GraphBuilder postProcessBuilder;

	std::vector<std::string> rayGenOutputs;
	std::vector<std::string> traceStepOutputs;
	std::vector<std::string> postProcessInputs;

	// inserting the ray material
	ConstructRayGenExec(rayGenBuilder, rayGenOutputs);
	ConstructTraceExec(traceStepBuilder, traceStepOutputs);
	ConstructPostProcessExec(postProcessBuilder, postProcessInputs);

	// constructing the execution graphs
	mRayGenGraph = *rayGenBuilder.GenerateExecutionGraph(rayGenOutputs);
	mPostProcessGraph = *postProcessBuilder.GenerateExecutionGraph({ RAY_CALC_LUMINANCE_NODE });

	mRayGenExecList = mRayGenGraph.SortEntries();
	mPostProcessExecList = mPostProcessGraph.SortEntries();

	for (uint32_t i = 0; i < depth; i++)
	{
		mTraceGraphs.emplace_back(*traceStepBuilder.GenerateExecutionGraph(traceStepOutputs));
		mTraceExecList.emplace_back(mTraceGraphs[i].SortEntries());
	}

	mDebugMode = !mDebugMode;
	SetDebugMode(!mDebugMode);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::SetTraceSession(const TraceSession& traceSession)
{
	_STL_ASSERT(traceSession.GetState() != TraceSessionState::eOpenScope,
		"Can't execute the trace session in the eOpenScope state!");

	mExecutorInfo->TracingSession = traceSession;
	mExecutorInfo->TracingInfo = traceSession.mSessionInfo->TraceInfo;

	auto& pipelines = mExecutorInfo->PipelineResources;

	pipelines.RayGenerator.mCamera = traceSession.mSessionInfo->CameraSpecsBuffer;
	pipelines.RayGenerator.mRays = mExecutorInfo->Rays;
	pipelines.RayGenerator.mSceneInfo = mExecutorInfo->Scene;
	pipelines.RayGenerator.mRayInfos = mExecutorInfo->RayInfos;

	pipelines.IntersectionPipeline.mCollisionInfos = mExecutorInfo->CollisionInfos;
	pipelines.IntersectionPipeline.mRays = mExecutorInfo->Rays;
	pipelines.IntersectionPipeline.mSceneInfo = mExecutorInfo->Scene;
	pipelines.IntersectionPipeline.mGeometryBuffers = traceSession.mSessionInfo->LocalBuffers;
	pipelines.IntersectionPipeline.mLightInfos = traceSession.mSessionInfo->LightInfos;
	pipelines.IntersectionPipeline.mLightProps = traceSession.mSessionInfo->LightPropsInfos;
	pipelines.IntersectionPipeline.mMeshInfos = traceSession.mSessionInfo->MeshInfos;

	pipelines.PrefixSummer.mRefCounts = mExecutorInfo->RefCounts;

	pipelines.RayRefCounter.mRayRefs = mExecutorInfo->RayRefs;
	pipelines.RayRefCounter.mRefCounts = mExecutorInfo->RefCounts;

	pipelines.RaySortPreparer.mCollisionInfos = mExecutorInfo->CollisionInfos;
	pipelines.RaySortPreparer.mRayRefs = mExecutorInfo->RayRefs;
	pipelines.RaySortPreparer.mRays = mExecutorInfo->Rays;
	pipelines.RaySortPreparer.mRaysInfos = mExecutorInfo->RayInfos;

	pipelines.RaySortFinisher.mRays = mExecutorInfo->Rays;
	pipelines.RaySortFinisher.mRayRefs = mExecutorInfo->RayRefs;
	pipelines.RaySortFinisher.mCollisionInfos = mExecutorInfo->CollisionInfos;
	pipelines.RaySortFinisher.mRaysInfos = mExecutorInfo->RayInfos;

	AssignMaterialsResources(pipelines.InactiveRayShader, *traceSession.mSessionInfo);

	pipelines.LuminanceMean.mPixelMean = mExecutorInfo->Target.PixelMean;
	pipelines.LuminanceMean.mPixelVariance = mExecutorInfo->Target.PixelVariance;
	pipelines.LuminanceMean.mPresentable = mExecutorInfo->Target.Presentable;
	pipelines.LuminanceMean.mRays = mExecutorInfo->Rays;
	pipelines.LuminanceMean.mRayInfos = mExecutorInfo->RayInfos;
	pipelines.LuminanceMean.mSceneInfo = mExecutorInfo->Scene;

	pipelines.PostProcessor.mPresentable = mExecutorInfo->Target.Presentable;

	InvalidateMaterialData();

	pipelines.RayGenerator.UpdateDescriptors();
	pipelines.IntersectionPipeline.UpdateDescriptors();
	pipelines.PrefixSummer.UpdateDescriptors();
	pipelines.RayRefCounter.UpdateDescriptors();
	pipelines.RaySortPreparer.UpdateDescriptors();
	pipelines.RaySortFinisher.UpdateDescriptors();
	pipelines.LuminanceMean.UpdateDescriptors();
	pipelines.PostProcessor.UpdateDescriptors();
	pipelines.InactiveRayShader.UpdateDescriptors();

	UpdateMaterialDescriptors();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::SetSkybox(
	vkLib::ImageView view, vkLib::Core::Ref<vk::Sampler> sampler)
{
	mSkyboxExists = view || sampler;

	mSkyboxView = view;
	mSkyboxSampler = sampler;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::SetCameraView(const glm::mat4& cameraView)
{
	if (!mExecutorInfo->TracingSession.mSessionInfo)
		return;

	if (mTraceState == TraceSessionState::eTracing)
		mTraceState = TraceSessionState::eReady;

	mExecutorInfo->TracingInfo.CameraView = cameraView;
	mExecutorInfo->TracingSession.SetCameraView(cameraView);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ConnectRayGenToTrace(const std::vector<std::string>& traceInput)
{
	if(!mTraceGraphs.empty())
		EXEC_NAMESPACE::SerializeExecutionWavefronts(mCtx, { mRayGenGraph, mTraceGraphs.front() }, { traceInput });
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ConnectTraces(uint32_t leadingIdx,
	uint32_t dependentIdx, const std::vector<std::string>& traceInputs)
{
	EXEC_NAMESPACE::SerializeExecutionWavefronts(mCtx, { mTraceGraphs[leadingIdx], 
		mTraceGraphs[dependentIdx] }, { traceInputs });
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ConnectTraceToPostProcess(const std::vector<std::string>& postInputs)
{
	if(!mTraceGraphs.empty())
		EXEC_NAMESPACE::SerializeExecutionWavefronts(mCtx, { mTraceGraphs.back(), mPostProcessGraph }, { postInputs });
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::JoinGraphs()
{
	ConnectRayGenToTrace({ RAY_INTERSECTION_TEST_NODE });
	ConnectTraceToPostProcess({ RAY_CALC_LUMINANCE_NODE });

	for (uint32_t i = 1; i < mMaxBounce; i++)
		ConnectTraces(i - 1, i, { RAY_INTERSECTION_TEST_NODE });
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::SeparateGraphs()
{
	mRayGenGraph.ClearInputInjections();
	mRayGenGraph.ClearOutputInjections();

	mPostProcessGraph.ClearInputInjections();
	mPostProcessGraph.ClearOutputInjections();

	for (uint32_t i = 0; i < mMaxBounce; i++)
	{
		mTraceGraphs[i].ClearInputInjections();
		mTraceGraphs[i].ClearOutputInjections();
	}
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ConstructTraceExec(
	EXEC_NAMESPACE::GraphBuilder& builder, std::vector<std::string>& outputs)
{
	outputs.clear();
	outputs.reserve(mExecutorInfo->MaterialResources.size() + 1);

	builder.Clear();
	builder.SetCtx(mCtx);

	std::string intersectionName = RAY_INTERSECTION_TEST_NODE;
	std::string materialName = RAY_MATERIAL_NODE;
	std::string emptyMaterial = RAY_EMPTY_MATERIAL_NODE;

	builder.InsertPipelineOp(intersectionName, mExecutorInfo->PipelineResources.IntersectionPipeline);

	builder[intersectionName].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
	{
		EXEC_NAMESPACE::CBScope executioner(cmd);
		RecordIntersectionTester(cmd, mExecutionBlock.mActiveBuffer);
	});

	uint32_t instanceIdx = 0;

	for (const auto& materialInstance : mExecutorInfo->MaterialResources)
	{
		std::string instanceName = materialName + std::to_string(instanceIdx);

		builder[instanceName] = materialInstance.GetMaterial();

		builder[instanceName].SetOpFn([this, instanceIdx](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
		{
			EXEC_NAMESPACE::CBScope executioner(cmd);
			RecordMaterialPipeline(cmd, instanceIdx, mExecutionBlock.mBounceIdx - 1, mExecutionBlock.mActiveBuffer);
		});

		instanceIdx++;

		builder.InsertDependency(intersectionName, instanceName);
		outputs.push_back(instanceName);
	}

	builder[emptyMaterial] = mExecutorInfo->PipelineResources.InactiveRayShader.GetMaterial();

	builder[emptyMaterial].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
	{
		EXEC_NAMESPACE::CBScope executioner(cmd);
		RecordMaterialPipeline(cmd, -1, mExecutionBlock.mBounceIdx - 1, mExecutionBlock.mActiveBuffer);
	});

	builder.InsertDependency(intersectionName, emptyMaterial);

	outputs.push_back(emptyMaterial);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ConstructRayGenExec(
	EXEC_NAMESPACE::GraphBuilder& builder, std::vector<std::string>& outputs)
{
	std::string rayGenerationName = RAY_GEN_NODE;

	outputs.clear();
	outputs.push_back(rayGenerationName);

	builder.Clear();
	builder.SetCtx(mCtx);

	builder.InsertPipelineOp(rayGenerationName, mExecutorInfo->PipelineResources.RayGenerator);

	builder[rayGenerationName].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
	{
		EXEC_NAMESPACE::CBScope executioner(cmd);
		RecordRayGenerator(cmd, mExecutionBlock.mActiveBuffer);
	});
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ConstructPostProcessExec(
	EXEC_NAMESPACE::GraphBuilder& builder, std::vector<std::string>& inputs)
{
	std::string postProcessName = RAY_POST_PROCESS_NODE;
	std::string luminanceName = RAY_CALC_LUMINANCE_NODE;

	inputs.clear();
	inputs.push_back(luminanceName);

	builder.Clear();
	builder.SetCtx(mCtx);

	builder.InsertPipelineOp(luminanceName, mExecutorInfo->PipelineResources.LuminanceMean);
	builder.InsertPipelineOp(postProcessName, mExecutorInfo->PipelineResources.PostProcessor);

	builder[luminanceName].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
	{
		EXEC_NAMESPACE::CBScope executioner(cmd);
		RecordLuminanceMean(cmd, mExecutionBlock.mActiveBuffer);
	});

	builder[postProcessName].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
	{
		EXEC_NAMESPACE::CBScope executioner(cmd);
		RecordPostProcess(cmd);
	});

	builder.InsertDependency(luminanceName, postProcessName);
}

uint32_t AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::GetRandomNumber()
{
	uint32_t Random = mExecutorInfo->UniformDistribution(mExecutorInfo->RandomEngine);
	_STL_ASSERT(Random != 0, "Random number can't be zero!");

	return Random;
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceResult AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::StepImpl()
{
	if (mExecutionBlock.mBounceIdx > mMaxBounce)
	{
		ExecuteGraphList(mPostProcessExecList);
		mExecutionBlock = {};
		return TraceResult::eComplete;
	}

	if (mExecutionBlock.mBounceIdx == 0)
	{
		Reset();
		UpdateSceneInfo();
		ExecuteGraphList(mRayGenExecList);
		mExecutionBlock.mBounceIdx++;
		return TraceResult::ePending;
	}

	ExecuteGraphList(mTraceExecList[mExecutionBlock.mBounceIdx - 1]);

	mExecutionBlock.mBounceIdx++;

	return TraceResult::eTraversing;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordRayGenerator(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer)
{
	auto workGroupSize = mExecutorInfo->PipelineResources.RayGenerator.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.RayGenerator.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.RayGenerator.Activate();

	Aqua::PushConst(mExecutorInfo->PipelineResources.RayGenerator, "eCompute.Camera.Index_0", mExecutorInfo->TracingInfo.CameraView);
	Aqua::PushConst(mExecutorInfo->PipelineResources.RayGenerator, "eCompute.Camera.Index_1", GetRandomNumber());
	Aqua::PushConst(mExecutorInfo->PipelineResources.RayGenerator, "eCompute.Camera.Index_2", pActiveBuffer);

	mExecutorInfo->PipelineResources.RayGenerator.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.RayGenerator.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordRaySortFinisher(vk::CommandBuffer commandBuffer)
{
	auto workGroupSize = mExecutorInfo->PipelineResources.RaySortFinisher.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.RaySortFinisher.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.RaySortFinisher.Activate();

	Aqua::PushConst(mExecutorInfo->PipelineResources.RaySortFinisher, "eCompute.RayData.Index_0", pRayCount);
	Aqua::PushConst(mExecutorInfo->PipelineResources.RaySortFinisher, "eCompute.RayData.Index_1", mExecutionBlock.mActiveBuffer);
	Aqua::PushConst(mExecutorInfo->PipelineResources.RaySortFinisher, "eCompute.RayData.Index_2", 0 /*comes from the sorting rays*/ );

	mExecutorInfo->PipelineResources.RaySortFinisher.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.RaySortFinisher.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordPrefixSummer(vk::CommandBuffer commandBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	mExecutorInfo->PipelineResources.PrefixSummer.Begin(commandBuffer);
	mExecutorInfo->PipelineResources.PrefixSummer.Activate();

	Aqua::PushConst(mExecutorInfo->PipelineResources.PrefixSummer, "eCompute.MetaData.Index_0", pMaterialCount);

	mExecutorInfo->PipelineResources.PrefixSummer.Dispatch({ 1, 1, 1 });

	mExecutorInfo->PipelineResources.PrefixSummer.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordRayCounter(vk::CommandBuffer commandBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	auto workGroupSize = mExecutorInfo->PipelineResources.RayRefCounter.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.RayRefCounter.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.RayRefCounter.Activate();

	Aqua::PushConst(mExecutorInfo->PipelineResources.RayRefCounter, "eCompute.MetaData.Index_0", pRayCount);
	Aqua::PushConst(mExecutorInfo->PipelineResources.RayRefCounter, "eCompute.MetaData.Index_1", pMaterialCount);

	mExecutorInfo->PipelineResources.RayRefCounter.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.RayRefCounter.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordRaySortPreparer(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	auto workGroupSize = mExecutorInfo->PipelineResources.RaySortPreparer.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.RaySortPreparer.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.RaySortPreparer.Activate();

	Aqua::PushConst(mExecutorInfo->PipelineResources.RaySortPreparer, "eCompute.RayData.Index_0", pRayCount);
	Aqua::PushConst(mExecutorInfo->PipelineResources.RaySortPreparer, "eCompute.RayData.Index_1", pActiveBuffer);

	mExecutorInfo->PipelineResources.RaySortPreparer.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.RaySortPreparer.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordIntersectionTester(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	auto workGroupSize = mExecutorInfo->PipelineResources.IntersectionPipeline.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.IntersectionPipeline.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.IntersectionPipeline.Activate();

	Aqua::PushConst(mExecutorInfo->PipelineResources.IntersectionPipeline, "eCompute.RayData.Index_0", pRayCount);
	Aqua::PushConst(mExecutorInfo->PipelineResources.IntersectionPipeline, "eCompute.RayData.Index_1", pActiveBuffer);

	mExecutorInfo->PipelineResources.IntersectionPipeline.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.IntersectionPipeline.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordLuminanceMean(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	auto workGroupSize = mExecutorInfo->PipelineResources.LuminanceMean.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.LuminanceMean.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.LuminanceMean.Activate();

	Aqua::PushConst(mExecutorInfo->PipelineResources.LuminanceMean, "eCompute.ShaderData.Index_0", pRayCount);
	Aqua::PushConst(mExecutorInfo->PipelineResources.LuminanceMean, "eCompute.ShaderData.Index_1", pActiveBuffer);

	mExecutorInfo->PipelineResources.LuminanceMean.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.LuminanceMean.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordPostProcess(vk::CommandBuffer commandBuffer)
{
	glm::uvec3 rayGroupSize = mExecutorInfo->PipelineResources.RayGenerator.GetWorkGroupSize();
	glm::uvec3 workGroups = { mExecutorInfo->CreateInfo.TileSize.x / rayGroupSize.x,
	mExecutorInfo->CreateInfo.TileSize.y / rayGroupSize.y, 1 };

	mExecutorInfo->PipelineResources.PostProcessor.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.PostProcessor.Activate();

	Aqua::PushConst(mExecutorInfo->PipelineResources.PostProcessor, "eCompute.ShaderData.Index_0", mExecutorInfo->CreateInfo.TileSize.x);
	Aqua::PushConst(mExecutorInfo->PipelineResources.PostProcessor, "eCompute.ShaderData.Index_1", mExecutorInfo->CreateInfo.TileSize.y);
	Aqua::PushConst(mExecutorInfo->PipelineResources.PostProcessor, "eCompute.ShaderData.Index_2", (uint32_t) (int) mPostProcess);

	mExecutorInfo->PipelineResources.PostProcessor.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.PostProcessor.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ExecuteGraphList(const EXEC_NAMESPACE::GraphList& execList)
{
	auto executor = mExecutorInfo->Workers;

	for (size_t i = 0; i < execList.size(); i++)
	{
		auto& op = *execList[i];

		size_t workerIdx = i % mExecutorInfo->Workers.size();
		auto worker = mExecutorInfo->Workers[workerIdx];

		worker.WaitIdle();
		op(mCmdBufs[workerIdx], worker);
	}
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::UpdateSceneInfo()
{
	WavefrontSceneInfo& sceneInfo = mExecutorInfo->TracingSession.mSessionInfo->SceneData;
	sceneInfo.ImageResolution = mExecutorInfo->CreateInfo.TargetResolution;
	sceneInfo.MinBound = { 0, 0 };
	sceneInfo.MaxBound = mExecutorInfo->CreateInfo.TargetResolution;
	sceneInfo.FrameCount = mTraceState == TraceSessionState::eReady ?
		1 : sceneInfo.FrameCount + 1;

	mExecutorInfo->Scene.Clear();
	mExecutorInfo->Scene << sceneInfo;

	mTraceState = TraceSessionState::eTracing;

	ShaderData shaderData{};
	shaderData.uRayCount = (uint32_t) mExecutorInfo->Rays.GetSize() / 2;
	shaderData.uSkyboxColor = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f);
	shaderData.uSkyboxExists = mSkyboxExists;

	mExecutorInfo->TracingSession.mSessionInfo->ShaderConstData.Clear();
	mExecutorInfo->TracingSession.mSessionInfo->ShaderConstData << shaderData;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::InvalidateMaterialData()
{
	if (!mExecutorInfo->TracingSession)
		return;

	auto& TracingSession = *mExecutorInfo->TracingSession.mSessionInfo;

	for (auto& curr : mExecutorInfo->MaterialResources)
		AssignMaterialsResources(curr, TracingSession);

	auto& inactivePipeline = mExecutorInfo->PipelineResources.InactiveRayShader;
	AssignMaterialsResources(inactivePipeline, TracingSession);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::AssignMaterialsResources(MaterialInstance& instance, const SessionInfo& TracingSession)
{
	instance[{ 0, 0, 0 }].SetStorageBuffer(mExecutorInfo->Rays.GetBufferRsc());
	instance[{ 0, 1, 0 }].SetStorageBuffer(mExecutorInfo->RayInfos.GetBufferRsc());
	instance[{ 0, 2, 0 }].SetStorageBuffer(mExecutorInfo->CollisionInfos.GetBufferRsc());
	instance[{ 0, 3, 0 }].SetStorageBuffer(TracingSession.LocalBuffers.Vertices.GetBufferRsc());
	instance[{ 0, 4, 0 }].SetStorageBuffer(TracingSession.LocalBuffers.Normals.GetBufferRsc());
	instance[{ 0, 5, 0 }].SetStorageBuffer(TracingSession.LocalBuffers.TexCoords.GetBufferRsc());
	instance[{ 0, 6, 0 }].SetStorageBuffer(TracingSession.LocalBuffers.Faces.GetBufferRsc());
	instance[{ 0, 7, 0 }].SetStorageBuffer(TracingSession.LightInfos.GetBufferRsc());
	instance[{ 0, 8, 0 }].SetStorageBuffer(TracingSession.LightPropsInfos.GetBufferRsc());
	instance[{ 1, 0, 0 }].SetUniformBuffer(TracingSession.ShaderConstData.GetBufferRsc());

	if(mSkyboxExists)
		instance[{ 0, 9, 0 }].SetSampledImage(mSkyboxView, mSkyboxSampler);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordMaterialPipeline(vk::CommandBuffer commandBuffer, uint32_t pMaterialRef, uint32_t pBounceIdx, uint32_t pActiveBuffer)
{
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / 256, 1, 1 };

	uint32_t MaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size());

	const vkLib::ComputePipeline* pipelinePtr = nullptr; 
	
	if (pMaterialRef != -1)
		pipelinePtr = reinterpret_cast<const vkLib::ComputePipeline*>(mExecutorInfo->MaterialResources[pMaterialRef].GetBasicPipeline());
	else
		pipelinePtr = reinterpret_cast<const vkLib::ComputePipeline*>(mExecutorInfo->PipelineResources.InactiveRayShader.GetBasicPipeline());

	const vkLib::ComputePipeline& pipeline = *pipelinePtr;

	pipeline.Begin(commandBuffer);

	pipeline.Activate();

	Aqua::PushConst(pipeline, "eCompute.ShaderConstants.Index_0", pMaterialRef);
	Aqua::PushConst(pipeline, "eCompute.ShaderConstants.Index_1", pActiveBuffer);
	
	if(pMaterialRef != -1)
		Aqua::PushConst(pipeline, "eCompute.ShaderConstants.Index_2", GetRandomNumber());
	//pipeline.SetShaderConstant("eCompute.ShaderConstants.Index_3", pBounceIdx);

	pipeline.Dispatch(workGroups);

	pipeline.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::UpdateMaterialDescriptors()
{
	for (auto& instance : mExecutorInfo->MaterialResources)
	{
		instance.UpdateDescriptors();
	}

	mExecutorInfo->PipelineResources.InactiveRayShader.UpdateDescriptors();
}

