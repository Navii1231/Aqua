#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/BackEndGraph.h"
#include "DeferredRenderer/Renderer/Environment.h"
#include "Execution/GenericDraft.h"
#include "DeferredRenderer/Pipelines/TextureVisualizer.h"
#include "Utils/CompilerErrorChecker.h"

AQUA_BEGIN

struct BackEndGraphConfig
{
	RendererFeatureFlags mFeatures;

	EXEC_NAMESPACE::Wavefront mInputs;

	FeatureInfoMap mFeatureInfos;
	vkLib::Buffer<CameraInfo> mCamera;

	EnvironmentRef mEnv;
	Mat4Buf mModels;

	vkLib::Framebuffer mShadingBuffer;
	vkLib::Framebuffer mPostProcessingBuffer;

	RenderTargetFactory mFramebufferFactory;

	vkLib::ResourcePool mResourcePool;

	// SSAO, Bloom effect, screen space reflections, post processing effects, skybox stuff
	// motion blur
	EXEC_NAMESPACE::GenericDraft mGraphBuilder;

	vkLib::Core::Ref<vk::Sampler> mPostProcessSampler;

	vkLib::Context mCtx;

	// Shader stuff...
	std::filesystem::path mShaderDirectory;
	vkLib::PShader mPostProcessingShader;
};

AQUA_END

AQUA_NAMESPACE::BackEndGraph::BackEndGraph()
{
	mConfig = std::make_shared<BackEndGraphConfig>();

	SetupShaders();
}

void AQUA_NAMESPACE::BackEndGraph::SetShaderDirectory(const std::filesystem::path& shaderDir)
{
	mConfig->mShaderDirectory = shaderDir;

	SetupShaders();
}

void AQUA_NAMESPACE::BackEndGraph::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;
	mConfig->mFramebufferFactory.SetContextBuilder(ctx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));
	mConfig->mResourcePool = ctx.CreateResourcePool();

	mConfig->mPostProcessSampler = mConfig->mResourcePool.CreateSampler({});

	mConfig->mGraphBuilder.SetCtx(ctx);
}

void AQUA_NAMESPACE::BackEndGraph::SetEnvironment(EnvironmentRef env)
{
	mConfig->mEnv = env;
}

void AQUA_NAMESPACE::BackEndGraph::SetFeaturesInfos(const FeatureInfoMap& featureInfo)
{
	mConfig->mFeatureInfos = featureInfo;
}

void AQUA_NAMESPACE::BackEndGraph::SetFeatureFlags(RendererFeatureFlags flags)
{
	mConfig->mFeatures = flags;
}

void AQUA_NAMESPACE::BackEndGraph::PrepareFeatures()
{
	mConfig->mGraphBuilder.Clear();
	mConfig->mInputs.clear();

	PrepareSSAO();
	PrepareBloomEffect();
	PreparePostProcessing();
}

void AQUA_NAMESPACE::BackEndGraph::PrepareSSAO()
{
	if ((mConfig->mFeatures & RenderingFeature::eSSAO) == RendererFeatureFlags(0))
		return;

}

void AQUA_NAMESPACE::BackEndGraph::PrepareBloomEffect()
{
	if ((mConfig->mFeatures & RenderingFeature::eBloomEffect) == RendererFeatureFlags(0))
		return;
}

void AQUA_NAMESPACE::BackEndGraph::PreparePostProcessing()
{
	mConfig->mGraphBuilder.SubmitOperation(-1);
	mConfig->mInputs.emplace_back(-1);
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Graph AQUA_NAMESPACE::BackEndGraph::CreateGraph()
{
	auto config = mConfig;
	auto pipelineBuilder = mConfig->mCtx.MakePipelineBuilder();

	auto graph = *mConfig->mGraphBuilder.Construct({ EXEC_NAMESPACE::NodeID(-1) });

	graph.InsertPipeOp(-1, pipelineBuilder.BuildGraphicsPipeline<TextureVisualizer>(mConfig->mPostProcessingShader, mConfig->mPostProcessingBuffer));

	ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[-1]).Fn = [config](vk::CommandBuffer cmd, const EXEC_NAMESPACE::GenericNode* op)
		{
			EXEC_NAMESPACE::CBScope exec(cmd);

			auto& pipeline = *(TextureVisualizer*)GetRefAddr(op->GFX);
			auto image = *config->mShadingBuffer.GetColorAttachments().front();

			image.BeginCommands(cmd);
			image.RecordTransitionLayout(vk::ImageLayout::eGeneral);

			pipeline.Begin(cmd);

			pipeline.Activate();
			pipeline.DrawVertices(0, 0, 1, 6);

			pipeline.End();

			image.EndCommands();
		};

	ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[-1]).UpdateFn = [config](EXEC_NAMESPACE::GenericNode* op)
		{
			auto& pipeline = *reinterpret_cast<TextureVisualizer*>(GetRefAddr(op->GFX));

			pipeline.UpdateTexture(config->mShadingBuffer.GetColorAttachments().front(), config->mPostProcessSampler);
		};

	return graph;
}

void AQUA_NAMESPACE::BackEndGraph::SetModels(Mat4Buf models)
{
	mConfig->mModels = models;
}

void AQUA_NAMESPACE::BackEndGraph::PrepareFramebuffers(const glm::uvec2& rendererResolution)
{
	auto& postProcessFac = mConfig->mFramebufferFactory;

	postProcessFac.AddColorAttribute("ColorOutput", "RGBA8UN");
	postProcessFac.SetAllColorProperties(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment);

	auto error = postProcessFac.Validate();

	_STL_ASSERT(error, "can't validate the post processing framebuffer");

	postProcessFac.SetTargetSize(rendererResolution);
	mConfig->mPostProcessingBuffer = *postProcessFac.CreateFramebuffer();
}

void AQUA_NAMESPACE::BackEndGraph::SetShadingFrameBuffer(vkLib::Framebuffer framebuffer)
{
	mConfig->mShadingBuffer = framebuffer;
}

void AQUA_NAMESPACE::BackEndGraph::SetupShaders()
{
	mConfig->mPostProcessingShader.SetFilepath("eVertex", (mConfig->mShaderDirectory / "PostProcess.vert").string());
	mConfig->mPostProcessingShader.SetFilepath("eFragment", (mConfig->mShaderDirectory / "PostProcess.frag").string());

	auto errors = mConfig->mPostProcessingShader.CompileShaders();
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Wavefront AQUA_NAMESPACE::BackEndGraph::GetInputs() const
{
	return mConfig->mInputs;
}

vkLib::Framebuffer AQUA_NAMESPACE::BackEndGraph::GetPostprocessbuffer() const
{
	return mConfig->mPostProcessingBuffer;
}
