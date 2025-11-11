#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/Renderer.h"
#include "DeferredRenderer/Pipelines/DeferredPipeline.h"
#include "DeferredRenderer/Pipelines/ShadowPipeline.h"
#include "DeferredRenderer/Pipelines/SkyboxPipeline.h"
#include "DeferredRenderer/Pipelines/TextureVisualizer.h"

#include "DeferredRenderer/Pipelines/ClearPipeline.h"

#include "DeferredRenderer/Renderable/CopyIndices.h"

#include "DeferredRenderer/Renderable/RenderTargetFactory.h"

#include "DeferredRenderer/Renderer/FrontEndGraph.h"
#include "DeferredRenderer/Renderer/BackEndGraph.h"
#include "DeferredRenderer/Renderer/MaterialSystem.h"
#include "DeferredRenderer/Renderer/BezierCurve.h"
#include "DeferredRenderer/Renderer/RenderableManagerie.h"

#include "DeferredRenderer/Renderable/RenderableBuilder.h"

#include "Utils/CompilerErrorChecker.h"
#include <semaphore>

AQUA_BEGIN

struct RendererConfig
{
	RendererConfig() = default;
	~RendererConfig() = default;

	RenderableManagerie mRenderableManagerie;
	// environment info
	EnvironmentRef mEnv;

	// features info (shadow mapping, SSAO, motion blur, bloom effect_
	// todo: needs to be further developed
	std::unordered_map<RenderingFeature, FeatureInfo> mFeatureInfos;

	vkLib::Buffer<FeaturesEnabled> mFeatures;
	vkLib::Buffer<CameraInfo> mCamera;

	RendererFeatureFlags mFeatureFlags;

	// Feature implementations
	// todo; maybe you write Feature base class with a polymorphic functionality
	// the rest of the features can be inherited
	FrontEndGraph mFrontEnd;
	BackEndGraph mBackEnd;

	// Framebuffer related stuff...
	vkLib::Framebuffer mGBuffer;
	vkLib::Framebuffer mShadingbuffer;

	glm::uvec2 mTargetSize = { 1024, 1024 };

	RenderTargetFactory mRenderCtxFactory;

	// clearing the framebuffer images manually
	ClearPipeline mColorClear;

	// samplers for lighting materials and depth mapping
	vkLib::Core::Ref<vk::Sampler> mShadingSampler;
	vkLib::Core::Ref<vk::Sampler> mDepthSampler;

	// default materials reside here...
	MaterialSystem mMaterialSystem;

	// Execution stuff...
	// characterized by geometry buffer and material arrays
	EXEC_NAMESPACE::Ensemble mRenderingPipeline; // combines front, shading and backend
	EXEC_NAMESPACE::GraphList mDrawList;

	DeferredPipeline mDeferredPipeline;
	SkyboxPipeline mSkyboxPipeline;

	// CPU synchronization for cmd buffers
	std::vector<EXEC_NAMESPACE::ExecutionUnit> mDrawWorkers;

	vkLib::Core::Worker mRendererWorker;
	vkLib::CommandBufferAllocator mCmdAlloc;


	// vkLib resources
	vkLib::PipelineBuilder mPipelineBuilder;
	vkLib::ResourcePool mResourcePool;
	vkLib::Context mCtx;

	// Shader stuff...
	std::filesystem::path mShaderDirectory;
	vkLib::PShader mGBufferShader;
	vkLib::PShader mSkyboxShader;

	constexpr static uint64_t sGBufferID = -2;
};

void SetFeatures(RendererFeatureFlags flags, vkLib::Buffer<FeaturesEnabled> enabled, bool val)
{
	std::vector<FeaturesEnabled> features;

	enabled >> features;

	auto AssignFeature = [flags, val](RenderingFeature featFlag, uint32_t& featVal)
		{
			if (flags & RendererFeatureFlags(featFlag))
				featVal = val;
		};

	AssignFeature(RenderingFeature::eSSAO, features[0].SSAO);
	AssignFeature(RenderingFeature::eShadow, features[0].ShadowMapping);
	AssignFeature(RenderingFeature::eBloomEffect, features[0].BloomEffect);

	enabled.SetBuf(features.begin(), features.end());
}

void AddMaterialsIntoExecutionGraph(EXEC_NAMESPACE::GenericDraft& builder,
	const std::vector<Core::MaterialInfo>& materials, EXEC_NAMESPACE::NodeID& materialIdx, EXEC_NAMESPACE::NodeID offset)
{
	for (const auto& rawMaterial : materials)
	{
		builder.SubmitOperation(materialIdx + offset);

		if (materialIdx < materials.size() - 1)
		{
			builder.Connect(materialIdx + offset,
				materialIdx + offset + 1, vk::PipelineStageFlagBits::eFragmentShader);

		}

		materialIdx++;
	}
}

AQUA_END

AQUA_NAMESPACE::Renderer::Renderer()
{
	mConfig = std::make_shared<RendererConfig>();
	SetupShaders();
}

AQUA_NAMESPACE::Renderer::Renderer(vkLib::Context ctx, const std::filesystem::path& shaderDir)
{
	mConfig = std::make_shared<RendererConfig>();

	SetCtx(ctx);
	SetShaderDirectory(shaderDir);
}

AQUA_NAMESPACE::Renderer::~Renderer()
{
	if (!mConfig)
		return;
}

void AQUA_NAMESPACE::Renderer::SetShaderDirectory(const std::filesystem::path& shaderDir)
{
	mConfig->mShaderDirectory = shaderDir;

	mConfig->mFrontEnd.SetShaderDirectory(shaderDir);
	mConfig->mBackEnd.SetShaderDirectory(shaderDir);

	SetupShaders();

	mConfig->mMaterialSystem.SetShaderDirectory(shaderDir);
}

AQUA_NAMESPACE::RendererFeatureFlags AQUA_NAMESPACE::Renderer::GetEnabledFeatures()
{
	return mConfig->mFeatureFlags;
}

void AQUA_NAMESPACE::Renderer::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;
	mConfig->mRendererWorker = mConfig->mCtx.FetchWorker(0);

	mConfig->mPipelineBuilder = ctx.MakePipelineBuilder();
	mConfig->mResourcePool = mConfig->mCtx.CreateResourcePool();
	mConfig->mRenderCtxFactory.SetContextBuilder(ctx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));

	mConfig->mFrontEnd.SetCtx(ctx);
	mConfig->mBackEnd.SetCtx(ctx);

	mConfig->mCmdAlloc = ctx.CreateCommandPools()[0];
	mConfig->mDrawWorkers = mConfig->mCmdAlloc.CreateExecUnits(16);

	SetupFeatureInfos();

	mConfig->mCamera = mConfig->mResourcePool.CreateBuffer<CameraInfo>(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
	mConfig->mFeatures = mConfig->mResourcePool.CreateBuffer<FeaturesEnabled>(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mCamera.Resize(1);
	mConfig->mFeatures.Resize(1);

	vkLib::SamplerInfo depthSamplerInfo{};
	depthSamplerInfo.MagFilter = vk::Filter::eNearest;
	depthSamplerInfo.MinFilter = vk::Filter::eNearest;

	mConfig->mShadingSampler = mConfig->mResourcePool.CreateSampler({});
	mConfig->mDepthSampler = mConfig->mResourcePool.CreateSampler(depthSamplerInfo);

	mConfig->mMaterialSystem.SetCtx(ctx);
	mConfig->mRenderableManagerie.SetCtx(ctx);

	UpdateRenderableManagerie();
	mConfig->mFrontEnd.SetVertexFactory(mConfig->mRenderableManagerie.GetVertexFactory());
}

void AQUA_NAMESPACE::Renderer::EnableFeatures(RendererFeatureFlags flags)
{
	mConfig->mFeatureFlags |= flags;
	SetFeatures(flags, mConfig->mFeatures, true);
	mConfig->mFrontEnd.SetFeatureFlags(mConfig->mFeatureFlags);
	mConfig->mBackEnd.SetFeatureFlags(mConfig->mFeatureFlags);
}

void AQUA_NAMESPACE::Renderer::DisableFeatures(RendererFeatureFlags flags)
{
	mConfig->mFeatureFlags &= RendererFeatureFlags(~(uint64_t)flags);
	SetFeatures(flags, mConfig->mFeatures, false);
	mConfig->mFrontEnd.SetFeatureFlags(mConfig->mFeatureFlags);
	mConfig->mBackEnd.SetFeatureFlags(mConfig->mFeatureFlags);
}

void AQUA_NAMESPACE::Renderer::SetShadingbuffer(vkLib::Framebuffer framebuffer)
{
	mConfig->mShadingbuffer = framebuffer;
	mConfig->mMaterialSystem.SetHyperSurfRenderProperties(framebuffer.GetParentContext());

	// setting the clear color pipelines
	mConfig->mColorClear = mConfig->mPipelineBuilder.BuildComputePipeline<ClearPipeline>("rgba32f", glm::uvec2(16, 16));
	UpdateRenderableManagerie();

	mConfig->mTargetSize = framebuffer.GetResolution();
}

void AQUA_NAMESPACE::Renderer::ResetRenderTarget(const glm::uvec2& resolution)
{
	mConfig->mShadingbuffer = {};
	mConfig->mTargetSize = resolution;
}

void AQUA_NAMESPACE::Renderer::SetSSAOConfig(const SSAOFeature& config)
{
	mConfig->mFeatureInfos[RenderingFeature::eSSAO].UniBuffer.SetBuf(&config, &config + 1, 0);
}

void AQUA_NAMESPACE::Renderer::SetShadowConfig(const ShadowCascadeFeature& config)
{
	mConfig->mFeatureInfos[RenderingFeature::eShadow].UniBuffer.SetBuf(&config, &config + 1, 0);
	mConfig->mFrontEnd.SetShadowFeature(config);
}

void AQUA_NAMESPACE::Renderer::SetBloomEffectConfig(const BloomEffectFeature& config)
{
	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].UniBuffer.SetBuf(&config, &config + 1);
}

void AQUA_NAMESPACE::Renderer::SetEnvironment(EnvironmentRef env)
{
	// Making sure that the environment has buffers
	// TODO: may break if we're dealing with data across multiple queue families...
	env->RegenerateBuffers(mConfig->mResourcePool);
	mConfig->mEnv = env;

	mConfig->mFrontEnd.SetEnvironment(env);
	mConfig->mBackEnd.SetEnvironment(env);

	UpdateRenderableManagerie();
}

void AQUA_NAMESPACE::Renderer::SetCamera(const glm::mat4& projection, const glm::mat4& view)
{
	CameraInfo camera{ projection, view };
	mConfig->mCamera.SetBuf(&camera, &camera + 1);
}

void AQUA_NAMESPACE::Renderer::PrepareFeatures()
{
	// creating new framebuffer if not provided
	if (!mConfig->mShadingbuffer)
	{
		RenderTargetFactory targetFac;

		targetFac.SetContextBuilder(mConfig->mCtx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));

		targetFac.AddColorAttribute("Color", "RGBA32F");
		targetFac.SetDepthAttribute("Depth", "D24Un_S8U");

		targetFac.SetAllColorProperties(vk::AttachmentLoadOp::eLoad, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
		targetFac.SetDepthProperties(vk::AttachmentLoadOp::eLoad, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);

		targetFac.Validate();

		targetFac.SetTargetSize(mConfig->mTargetSize);

		SetShadingbuffer(*targetFac.CreateFramebuffer());
	}

	if (!mConfig->mEnv)
	{
		SetEnvironment(Aqua::MakeRef<Aqua::Environment>());
	}

	mConfig->mEnv->Update();

	mConfig->mFrontEnd.PrepareFramebuffers(mConfig->mShadingbuffer.GetResolution());
	mConfig->mBackEnd.PrepareFramebuffers(mConfig->mShadingbuffer.GetResolution());
	mConfig->mBackEnd.SetShadingFrameBuffer(mConfig->mShadingbuffer);

	mConfig->mFrontEnd.PrepareFeatures();
	mConfig->mBackEnd.PrepareFeatures();

	// creating geometry buffer
	PrepareFramebuffers();

	// creating essential pipelines
	mConfig->mDeferredPipeline = mConfig->mPipelineBuilder.BuildGraphicsPipeline<DeferredPipeline>(mConfig->mGBufferShader, mConfig->mGBuffer, mConfig->mRenderableManagerie.GetVertexBindingInfo());

	mConfig->mSkyboxPipeline = mConfig->mPipelineBuilder.BuildGraphicsPipeline<SkyboxPipeline>(mConfig->mSkyboxShader, mConfig->mShadingbuffer);
}

void AQUA_NAMESPACE::Renderer::InsertPreEventDependency(vkLib::Core::Ref<vk::Semaphore> signal)
{
	auto graphList = mConfig->mDrawList;
	auto graph = mConfig->mRenderingPipeline;

	if (graphList.empty())
		return;

	EXEC_NAMESPACE::DependencyInjection inInj{};
	inInj.Connect(graphList.front()->NodeId);
	inInj.SetSignal(signal);
	inInj.SetWaitPoint({});

	_STL_VERIFY(graph.Fetch(0).InjectInputDependencies(inInj), "couldn't inject dependency");
}

void AQUA_NAMESPACE::Renderer::InsertPostEventDependency(vkLib::Core::Ref<vk::Semaphore> signal, vk::PipelineStageFlags pipelineFlags /*= vk::PipelineStageFlagBits::eTopOfPipe*/)
{
	auto graphList = mConfig->mDrawList;
	auto graph = mConfig->mRenderingPipeline;

	if (graphList.empty())
		return;

	EXEC_NAMESPACE::DependencyInjection outInj{};
	outInj.Connect(graphList.back()->NodeId);
	outInj.SetSignal(signal);
	outInj.SetWaitPoint({});

	_STL_VERIFY(graph.GetGraphs().back().InjectOutputDependencies(outInj), "couldn't inject dependency");
}

void AQUA_NAMESPACE::Renderer::SubmitRenderable(const std::string& name, const glm::mat4& model,
	Renderable renderable, MaterialInstance instance)
{
	mConfig->mRenderableManagerie.SubmitRenderable(name, model, renderable, instance);
}

void AQUA_NAMESPACE::Renderer::SubmitRenderable(const std::string& name, const glm::mat4& model, const MeshData& mesh, MaterialInstance instance)
{
	mConfig->mRenderableManagerie.SubmitRenderable(name, model, mesh, instance);
}

void AQUA_NAMESPACE::Renderer::SubmitRenderable(const RenderableSubmitInfo& submitInfo)
{
	mConfig->mRenderableManagerie.SubmitRenderable(submitInfo);
}

void AQUA_NAMESPACE::Renderer::SubmitLines(const std::string& lineIsland,
	const vk::ArrayProxy<Line>& lines, float thickness /*= 2.0f*/)
{
	mConfig->mRenderableManagerie.SubmitLines(lineIsland, lines, thickness);
}

void AQUA_NAMESPACE::Renderer::SubmitCurves(const std::string& curveIsland,
	const vk::ArrayProxy<Curve>& connections, float thickness /*= 2.0f*/)
{
	mConfig->mRenderableManagerie.SubmitCurves(curveIsland, connections, thickness);
}

void AQUA_NAMESPACE::Renderer::SubmitBezierCurves(const std::string& curveIsland,
	const vk::ArrayProxy<Curve>& curves, float thickness /*= 2.0f*/)
{
	mConfig->mRenderableManagerie.SubmitBezierCurves(curveIsland, curves, thickness);
}

void AQUA_NAMESPACE::Renderer::SubmitPoints(const std::string& pointIsland,
	const vk::ArrayProxy<Point>& points, float pointSize)
{
	mConfig->mRenderableManagerie.SubmitPoints(pointIsland, points, pointSize);
}

void AQUA_NAMESPACE::Renderer::RemoveRenderable(const std::string& name)
{
	mConfig->mRenderableManagerie.RemoveRenderable(name);
}

void AQUA_NAMESPACE::Renderer::ClearRenderables()
{
	// Resetting every state relating GPU shading network memory
	mConfig->mRenderableManagerie.ClearRenderables();
}

void AQUA_NAMESPACE::Renderer::PrepareMaterialNetwork()
{
	mConfig->mFrontEnd.SetModels(mConfig->mRenderableManagerie.GetModels());
	mConfig->mBackEnd.SetModels(mConfig->mRenderableManagerie.GetModels());

	auto shadingNetwork = PrepareShadingNetwork();

	mConfig->mRenderingPipeline = EXEC_NAMESPACE::Ensemble::Flatten(EXEC_NAMESPACE::Ensemble::MakeSeq(mConfig->mCtx, { mConfig->mFrontEnd.CreateGraph(), shadingNetwork, mConfig->mBackEnd.CreateGraph() }));

	mConfig->mDrawList = mConfig->mRenderingPipeline.SortEntries();

	uint32_t nodeCount = static_cast<uint32_t>(mConfig->mDrawList.size());
	mConfig->mDrawWorkers = mConfig->mCmdAlloc.CreateExecUnits(nodeCount);

	// update the descriptor resources
	UpdateMaterialData();
	mConfig->mRenderingPipeline.Update();
}

void AQUA_NAMESPACE::Renderer::ActivateRenderables(const vk::ArrayProxy<std::string>& names)
{
	mConfig->mRenderableManagerie.ActivateRenderables(names);
}

void AQUA_NAMESPACE::Renderer::ActivateAll()
{
	mConfig->mRenderableManagerie.ActivateAll();
}

void AQUA_NAMESPACE::Renderer::DeactivateRenderables(const vk::ArrayProxy<std::string>& names)
{
	mConfig->mRenderableManagerie.DeactivateRenderables(names);
}

void AQUA_NAMESPACE::Renderer::DeactivateAll()
{
	mConfig->mRenderableManagerie.DeactivateAll();
}


void AQUA_NAMESPACE::Renderer::ModifyRenderable(const std::string& name, const vk::ArrayProxy<Line>& lines)
{
	mConfig->mRenderableManagerie.ModifyRenderable(name, lines);
}

void AQUA_NAMESPACE::Renderer::ModifyRenderable(const std::string& name, const glm::mat4& modelMatrix)
{
	mConfig->mRenderableManagerie.ModifyRenderable(name, modelMatrix);
}

void AQUA_NAMESPACE::Renderer::ModifyRenderable(const std::string& name, Renderable renderable)
{
	mConfig->mRenderableManagerie.ModifyRenderable(name, renderable);
}

void AQUA_NAMESPACE::Renderer::ModifyRenderable(const std::string& name, const MeshData& renderable)
{
	mConfig->mRenderableManagerie.ModifyRenderable(name, renderable);
}

void AQUA_NAMESPACE::Renderer::ModifyRenderable(const std::string& name, const vk::ArrayProxy<Curve>& curves)
{
	mConfig->mRenderableManagerie.ModifyRenderable(name, curves);
}

void AQUA_NAMESPACE::Renderer::ModifyRenderable(const std::string& name, const vk::ArrayProxy<Point>& points)
{
	mConfig->mRenderableManagerie.ModifyRenderable(name, points);
}

void AQUA_NAMESPACE::Renderer::InvalidateBuffers()
{
	mConfig->mRenderableManagerie.InvalidateVertices();
}

void AQUA_NAMESPACE::Renderer::UpdateDescriptors()
{
	mConfig->mRenderingPipeline.Update();
}

void AQUA_NAMESPACE::Renderer::IssueDrawCall()
{
	// need a way to sync with the upload renderables routine
	EXEC_NAMESPACE::Execute(mConfig->mDrawList, mConfig->mDrawWorkers);
}

vk::Result AQUA_NAMESPACE::Renderer::WaitIdle(std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	auto result = mConfig->mRenderableManagerie.WaitIdle();

	if (result != vk::Result::eSuccess)
		return result;

	result = EXEC_NAMESPACE::WaitFor(mConfig->mDrawWorkers);

	if (result != vk::Result::eSuccess)
		return result;

	result = mConfig->mRendererWorker.WaitIdle();

	return result;
}

vkLib::Framebuffer AQUA_NAMESPACE::Renderer::GetPostprocessbuffer() const
{
	return mConfig->mBackEnd.GetPostprocessbuffer();
}

vkLib::Framebuffer AQUA_NAMESPACE::Renderer::GetShadingbuffer() const
{
	return mConfig->mShadingbuffer;
}

AQUA_NAMESPACE::VertexBindingMap AQUA_NAMESPACE::Renderer::GetVertexBindings() const
{
	return mConfig->mRenderableManagerie.GetVertexBindingInfo();
}

AQUA_NAMESPACE::ImageAttributeList AQUA_NAMESPACE::Renderer::GetShaderInputAttributes() const
{
	return mConfig->mRenderCtxFactory.GetColorAttributes();
}

vkLib::Framebuffer AQUA_NAMESPACE::Renderer::GetGBuffer() const
{
	return mConfig->mGBuffer;
}

vkLib::Framebuffer AQUA_NAMESPACE::Renderer::GetDepthbuffer() const
{
	return mConfig->mFrontEnd.GetDepthbuffers()[0];
}

vkLib::ImageView AQUA_NAMESPACE::Renderer::GetDepthView() const
{
	return mConfig->mFrontEnd.GetDepthViews()[0];
}

vk::Result AQUA_NAMESPACE::Renderer::WaitForWorker(uint32_t freeWorker, std::chrono::nanoseconds timeout /*= std::chrono::nanoseconds::max()*/)
{
	return mConfig->mDrawWorkers[freeWorker].Worker.WaitIdle(timeout);
}

void AQUA_NAMESPACE::Renderer::UpdateRenderableManagerie()
{
	mConfig->mRenderableManagerie.SetManagerieData(
		mConfig->mEnv, mConfig->mShadingbuffer, mConfig->mMaterialSystem);
}

Aqua::Exec::Graph AQUA_NAMESPACE::Renderer::PrepareShadingNetwork()
{
	// TODO: reading the required geometry resources from the materials
	// and producing the corresponding g buffers and mapping them to correct lighting passes

	EXEC_NAMESPACE::NodeID shadingIdx = 0;
	uint32_t matCount = 0;

	auto config = mConfig;

	EXEC_NAMESPACE::GenericDraft draft(mConfig->mCtx);

	draft.Clear();

	draft.SubmitOperation(0);
	draft.SubmitOperation(1);

	auto forwardIdxBegin = shadingIdx + 2;
	AddMaterialsIntoExecutionGraph(draft, mConfig->mRenderableManagerie.GetForwardMaterials(), shadingIdx, 2);

	auto deferIdxBegin = shadingIdx + 2;
	AddMaterialsIntoExecutionGraph(draft, mConfig->mRenderableManagerie.GetDeferredMaterials(), shadingIdx, 2);

	LinkShaderExecutionGraphs(draft, deferIdxBegin, shadingIdx);

	// the sky box
	draft.SubmitOperation(shadingIdx + 2);

	auto graph = *draft.Construct({ shadingIdx + 2 });

	for (const auto& rawMaterial : mConfig->mRenderableManagerie.GetForwardMaterials())
	{
		graph.InsertOperation(mConfig->mCtx, forwardIdxBegin, rawMaterial.Op);
		ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[forwardIdxBegin++]).OpID = Core::MaterialInfo::sMatTypeID;
	}

	for (const auto& rawMaterial : mConfig->mRenderableManagerie.GetDeferredMaterials())
	{
		graph.InsertOperation(mConfig->mCtx, deferIdxBegin, rawMaterial.Op);
		ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[deferIdxBegin++]).OpID = Core::MaterialInfo::sMatTypeID;
	}

	// todo: a bit inefficient since the geometry is relatively fixed for each material
	graph.InsertPipeOp(0, mConfig->mDeferredPipeline);
	ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[0]).OpID = RendererConfig::sGBufferID;

	ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[0]).Fn =
		[this](vk::CommandBuffer cmds, const EXEC_NAMESPACE::GenericNode* op)
	{
		EXEC_NAMESPACE::CBScope executioner(cmds);

		auto& pipeline = *op->GFX;

		pipeline.Begin(cmds);

		pipeline.Activate();
		pipeline.DrawIndexed(0, 0, 0, 1);

		pipeline.End();
	};

	ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[0]).UpdateFn = [config](EXEC_NAMESPACE::GenericNode* op)
	{
		auto& pipeline = *reinterpret_cast<DeferredPipeline*>(GetRefAddr(op->GFX));

		pipeline.SetClearDepthStencilValues(1.0f, 0);

		pipeline.SetClearColorValues(0, { 1.0f, 0.0f, 1.0f, 1.0f });

		VertexFactory vertFac = config->mRenderableManagerie.GetVertexFactory();

		// Fetching data from the vertex factory
		for (const auto& [idx, binding] : config->mRenderableManagerie.GetVertexBindingInfo())
		{
			pipeline.SetVertexBuffer(idx, vertFac[binding.Name]);
		}

		pipeline.SetIndexBuffer(vertFac.GetIndexBuffer());

		pipeline.UpdateCamera(config->mCamera);
		pipeline.UpdateModels(config->mRenderableManagerie.GetModels());
	};

	ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[1]).Fn = [this](vk::CommandBuffer buffer,
		const EXEC_NAMESPACE::GenericNode* op)
		{
			EXEC_NAMESPACE::CBScope executioner(buffer);

			mConfig->mColorClear(buffer, mConfig->mShadingbuffer.GetColorAttachments().front(), { 0.0f, 1.0f, 0.0f, 0.0f });
		};

	graph.InsertPipeOp(shadingIdx + 2, mConfig->mSkyboxPipeline);

	ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[shadingIdx + 2]).Fn =
		[this](vk::CommandBuffer cmds, const EXEC_NAMESPACE::GenericNode* op)
	{
		EXEC_NAMESPACE::CBScope executioner(cmds);

		if (!mConfig->mEnv->GetSkybox() || !mConfig->mEnv->GetSampler())
			return; // do nothing

		auto& pipeline = *op->GFX;

		pipeline.Begin(cmds);

		pipeline.Activate();
		pipeline.DrawVertices(0, 0, 1, 6);

		pipeline.End();
	};

	ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[shadingIdx + 2]).UpdateFn = [config](EXEC_NAMESPACE::GenericNode* op)
		{
		auto& pipeline = *reinterpret_cast<SkyboxPipeline*>(GetRefAddr(op->GFX));

			pipeline.SetClearDepthStencilValues(1.0f, 0);
			
			pipeline.UpdateCamera(config->mCamera);
			pipeline.UpdateEnvironmentTexture(*config->mEnv->GetSkybox(), config->mEnv->GetSampler());
		};

	return graph;
}

void AQUA_NAMESPACE::Renderer::LinkShaderExecutionGraphs(EXEC_NAMESPACE::GenericDraft& draft, EXEC_NAMESPACE::NodeID forwardID, EXEC_NAMESPACE::NodeID deferID)
{
	// start and end points of each material series
	std::vector<std::pair<EXEC_NAMESPACE::NodeID, EXEC_NAMESPACE::NodeID>> endpoints;

	auto insertEndPoints = [this, &endpoints](EXEC_NAMESPACE::NodeID matBegin,
		const std::vector<Core::MaterialInfo>& materials)
	{
		if (!materials.empty())
		{
			auto& syncPoint = endpoints.emplace_back(matBegin, matBegin + materials.size() - 1);
		}
	};

	insertEndPoints(deferID, mConfig->mRenderableManagerie.GetDeferredMaterials());
	insertEndPoints(forwardID, mConfig->mRenderableManagerie.GetForwardMaterials());

	for (size_t i = 1; i < endpoints.size(); i++)
	{
		draft.Connect(endpoints[i - 1].second, endpoints[i].first,
			vk::PipelineStageFlagBits::eFragmentShader);
	}

	if (!endpoints.empty())
	{
		draft.Connect(0, endpoints.front().first, vk::PipelineStageFlagBits::eTopOfPipe);
		draft.Connect(1, endpoints.front().first, vk::PipelineStageFlagBits::eTopOfPipe);
		draft.Connect(endpoints.back().second, draft.GetOpCount(),
			vk::PipelineStageFlagBits::eFragmentShader);
	}
	else
	{
		draft.Connect(0, draft.GetOpCount(), vk::PipelineStageFlagBits::eTopOfPipe);
		draft.Connect(1, draft.GetOpCount(), vk::PipelineStageFlagBits::eTopOfPipe);
	}
}

void AQUA_NAMESPACE::Renderer::PrepareFramebuffers()
{
	auto& rcFac = mConfig->mRenderCtxFactory;

	rcFac.Clear();

	// TODO: for now, these geometry buffer outputs are fixed. this can be easily modified later
	rcFac.AddColorAttribute(ENTRY_POSITION, "RGBA32F");
	rcFac.AddColorAttribute(ENTRY_NORMAL, "RGBA32F");
	rcFac.AddColorAttribute(ENTRY_TEXCOORDS, "RGBA32F");
	rcFac.AddColorAttribute(ENTRY_TANGENT, "RGBA32F");
	rcFac.AddColorAttribute(ENTRY_BITANGENT, "RGBA32F");

	rcFac.SetDepthAttribute("Depth", "D24UN_S8U");

	rcFac.SetAllColorProperties(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
	rcFac.SetDepthProperties(vk::ImageUsageFlagBits::eDepthStencilAttachment);

	auto error = rcFac.Validate();
	_STL_ASSERT(error, "Couldn't validate the render factory");

	rcFac.SetImageView("Depth", mConfig->mShadingbuffer.GetDepthStencilAttachment());

	mConfig->mGBuffer = *rcFac.CreateFramebuffer();
}

void AQUA_NAMESPACE::Renderer::SetupFeatureInfos()
{
	mConfig->mFeatureInfos[RenderingFeature::eShadow].Name = "ShadowStage";
	mConfig->mFeatureInfos[RenderingFeature::eShadow].Stage = RenderingStage::eFrontEnd;
	mConfig->mFeatureInfos[RenderingFeature::eShadow].Type = RenderingFeature::eShadow;
	mConfig->mFeatureInfos[RenderingFeature::eShadow].UniBuffer =
		mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].Name = "BloomEffectStage";
	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].Stage = RenderingStage::eBackEnd;
	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].Type = RenderingFeature::eBloomEffect;
	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].UniBuffer =
		mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mFeatureInfos[RenderingFeature::eSSAO].Name = "SSAOStage";
	mConfig->mFeatureInfos[RenderingFeature::eSSAO].Stage = RenderingStage::eBackEnd;
	mConfig->mFeatureInfos[RenderingFeature::eSSAO].Type = RenderingFeature::eSSAO;
	mConfig->mFeatureInfos[RenderingFeature::eSSAO].UniBuffer =
		mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mFeatureInfos[RenderingFeature::eMotionBlur].Name = "MotionBlurStage";
	mConfig->mFeatureInfos[RenderingFeature::eMotionBlur].Stage = RenderingStage::eFrontEnd;
	mConfig->mFeatureInfos[RenderingFeature::eMotionBlur].Type = RenderingFeature::eMotionBlur;
	mConfig->mFeatureInfos[RenderingFeature::eMotionBlur].UniBuffer =
		mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mFrontEnd.SetFeaturesInfos(mConfig->mFeatureInfos);
	mConfig->mBackEnd.SetFeaturesInfos(mConfig->mFeatureInfos);
}

void AQUA_NAMESPACE::Renderer::UpdateMaterialData()
{
	auto& gBuffer = mConfig->mGBuffer;
	auto depthViews = mConfig->mFrontEnd.GetDepthViews();
	auto sampler = mConfig->mShadingSampler;
	const auto& lightBuffers = mConfig->mEnv->GetLightBuffers();

	auto colorProps = mConfig->mRenderCtxFactory.GetColorImagePropMaps();

	// we set all the resources that the renderer holds
	// in case the any set binding isn't defined in the material pipeline code
	// we return and do nothing of the error. This is a feature, not a bug
	for (const auto& material : mConfig->mRenderableManagerie.GetDeferredMaterials())
	{
		auto& materialInfo = *material.Info;

		materialInfo.Resources[{0, 0, 0}].SetStorageImage(mConfig->mShadingbuffer.GetColorAttachments().front());

		for (const auto& [name, prop] : colorProps)
		{
			// TODO: we need a macro to fix the descriptor location of each geometry image attribute
			materialInfo.Resources[{ GBUFFER_RSC_SET_IDX, prop.AttachIdx + 1, 0 }].SetSampledImage(
				gBuffer.GetColorAttachments()[prop.AttachIdx], sampler);
		}

		if (mConfig->mFeatureFlags & RenderingFeature::eShadow)
		{
			for (uint32_t depthIdx = 0; depthIdx < static_cast<uint32_t>(depthViews.size()); depthIdx++)
			{
				materialInfo.Resources[{ GBUFFER_RSC_SET_IDX, DEPTH_BINDING, depthIdx }].SetSampledImage(
					depthViews[depthIdx], mConfig->mDepthSampler);
			}
		}

		// setting the camera and model matrices
		materialInfo.Resources[{ENV_RSC_SET_IDX, 0, 0}].SetStorageBuffer(lightBuffers.mDirLightBuf.GetBufferRsc());
		materialInfo.Resources[{ENV_RSC_SET_IDX, 1, 0}].SetStorageBuffer(lightBuffers.mPointLightBuf.GetBufferRsc());
		materialInfo.Resources[{ENV_RSC_SET_IDX, 2, 0}].SetStorageBuffer(lightBuffers.mDirCameraInfos.GetBufferRsc());
		materialInfo.Resources[{ENV_RSC_SET_IDX, 3, 0}].SetStorageBuffer(mConfig->mRenderableManagerie.GetModels().GetBufferRsc());
		materialInfo.Resources[{ENV_RSC_SET_IDX, 4, 0}].SetUniformBuffer(mConfig->mCamera.GetBufferRsc());
	}

	for (const auto& material : mConfig->mRenderableManagerie.GetForwardMaterials())
	{
		auto& materialInfo = *material.Info;

		if (material.mData.mFeedingCameraInfo)
			materialInfo.Resources[material.mData.mCameraLocation].SetUniformBuffer(mConfig->mCamera.GetBufferRsc());
		if (material.mData.mFeedingModelMatrices)
			materialInfo.Resources[material.mData.mModelMatrixLocation].SetStorageBuffer(
				mConfig->mRenderableManagerie.GetModels().GetBufferRsc());
	}
}

void AQUA_NAMESPACE::Renderer::SetupShaders()
{
	mConfig->mGBufferShader.SetFilepath("eVertex", (mConfig->mShaderDirectory / "Defer.vert").string());
	mConfig->mGBufferShader.SetFilepath("eFragment", (mConfig->mShaderDirectory / "Defer.frag").string());

	mConfig->mGBufferShader.CompileShaders();

	mConfig->mSkyboxShader.SetFilepath("eVertex", (mConfig->mShaderDirectory / "Skybox.vert").string());
	mConfig->mSkyboxShader.SetFilepath("eFragment", (mConfig->mShaderDirectory / "Skybox.frag").string());

	mConfig->mSkyboxShader.AddMacro("MATH_PI", std::to_string(glm::pi<float>()));
	mConfig->mSkyboxShader.CompileShaders();
}
