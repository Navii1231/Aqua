#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/FrontEndGraph.h"
#include "Execution/GenericDraft.h"
#include "DeferredRenderer/Pipelines/ShadowPipeline.h"
#include "Utils/CompilerErrorChecker.h"

AQUA_BEGIN

struct FrontEndGraphConfig
{
	RendererFeatureFlags mFeatures;
	ShadowCascadeFeature mShadowFeature;

	EXEC_NAMESPACE::Wavefront mOutputs;

	FeatureInfoMap mFeatureInfos;
	vkLib::Buffer<CameraInfo> mCamera;

	EnvironmentRef mEnv;
	Mat4Buf mModels;

	std::vector<vkLib::Framebuffer> mDepthBuffers;
	std::vector<vkLib::ImageView> mDepthViews;

	VertexFactory* mVertexFactory = nullptr;
	RenderTargetFactory mFramebufferFactory;

	// new
	EXEC_NAMESPACE::GenericDraft mDraft;

	vkLib::Context mCtx;

	std::filesystem::path mShaderDirectory;
	vkLib::PShader mDepthShader;
};

AQUA_END

AQUA_NAMESPACE::FrontEndGraph::FrontEndGraph()
{
	mConfig = std::make_shared<FrontEndGraphConfig>();

	SetShaderDirectory(mConfig->mShaderDirectory);
}

void AQUA_NAMESPACE::FrontEndGraph::SetShaderDirectory(const std::filesystem::path& shaderDir)
{
	mConfig->mShaderDirectory = shaderDir;

	SetupShaders();
}

void AQUA_NAMESPACE::FrontEndGraph::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;
	mConfig->mFramebufferFactory.SetContextBuilder(ctx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));

	mConfig->mDraft.SetCtx(ctx);
}

void AQUA_NAMESPACE::FrontEndGraph::SetEnvironment(EnvironmentRef env)
{
	mConfig->mEnv = env;
}

void AQUA_NAMESPACE::FrontEndGraph::SetFeaturesInfos(const FeatureInfoMap& featureInfo)
{
	mConfig->mFeatureInfos = featureInfo;
}

void AQUA_NAMESPACE::FrontEndGraph::SetFeatureFlags(RendererFeatureFlags flags)
{
	mConfig->mFeatures = flags;
}

void AQUA_NAMESPACE::FrontEndGraph::SetShadowFeature(const ShadowCascadeFeature& feature)
{
	mConfig->mShadowFeature = feature;
}

void AQUA_NAMESPACE::FrontEndGraph::SetVertexFactory(VertexFactory& factory)
{
	mConfig->mVertexFactory = &factory;
}

void AQUA_NAMESPACE::FrontEndGraph::PrepareFeatures()
{
	mConfig->mDraft.Clear();
	mConfig->mOutputs.clear();

	PrepareDepthCascades();
}

void AQUA_NAMESPACE::FrontEndGraph::PrepareDepthCascades()
{
	if ((mConfig->mFeatures & RenderingFeature::eShadow) == RendererFeatureFlags(0))
		return;

	for (uint32_t i = 0; i < static_cast<uint32_t>(mConfig->mEnv->GetDirLightCount()); i++)
	{
		mConfig->mDraft.SubmitOperation(i);
		mConfig->mOutputs.emplace_back(i);
	}
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Graph AQUA_NAMESPACE::FrontEndGraph::CreateGraph()
{
	// all m by n cascade network are both the inputs and outputs
	auto graph = *mConfig->mDraft.Construct(mConfig->mOutputs);

	auto config = mConfig;

	VertexBindingMap vertexBindings{};
	vertexBindings[0].AddAttribute(0, "RGB32F");
	vertexBindings[0].SetName(ENTRY_POSITION);
	vertexBindings[1].AddAttribute(1, "RGB32F");
	vertexBindings[1].SetName(ENTRY_METADATA);

	auto pipelineBuilder = mConfig->mCtx.MakePipelineBuilder();

	for (uint32_t i = 0; i < static_cast<uint32_t>(mConfig->mEnv->GetDirLightCount()); i++)
	{
		graph[i] = EXEC_NAMESPACE::GenericNode(i, EXEC_NAMESPACE::OpType::eGraphics);

		graph.InsertPipeOp(i, pipelineBuilder.BuildGraphicsPipeline<ShadowPipeline>(mConfig->mDepthShader, mConfig->mDepthBuffers[i], vertexBindings));

		ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[i]).Fn = [config, offset = i](vk::CommandBuffer buffer, const EXEC_NAMESPACE::GenericNode* op)
			{
				EXEC_NAMESPACE::CBScope exec(buffer);

				op->GFX->Begin(buffer);

				op->GFX->Activate();

				auto error = op->GFX->SetShaderConstant("eVertex.ShaderConstants.Index_0", offset).or_else(
					[](const vkLib::ShaderConstantError& constError)
					{
						return std::expected<bool, vkLib::ShaderConstantError>(true);
					});

				op->GFX->DrawIndexed(0, 0, 0, 1);

				op->GFX->End();
			};

		ConvertNode<EXEC_NAMESPACE::GenericNode>(graph[i]).UpdateFn = [config](EXEC_NAMESPACE::GenericNode* op)
			{
				auto& pipeline = *reinterpret_cast<ShadowPipeline*>(GetRefAddr(op->GFX));

				pipeline.SetClearDepthStencilValues(1.0f, 0);

				pipeline.SetVertexBuffer(0, (*config->mVertexFactory)[ENTRY_POSITION]);
				pipeline.SetVertexBuffer(1, (*config->mVertexFactory)[ENTRY_METADATA]);

				pipeline.SetIndexBuffer(config->mVertexFactory->GetIndexBuffer());

				pipeline.UpdateCamera(config->mEnv->GetLightBuffers().mDirCameraInfos);
				pipeline.UpdateModels(config->mModels);
			};
	}

	return graph;
}

void AQUA_NAMESPACE::FrontEndGraph::SetModels(Mat4Buf models)
{
	mConfig->mModels = models;
}

void AQUA_NAMESPACE::FrontEndGraph::PrepareFramebuffers(const glm::uvec2& rendererResolution)
{
	PrepareDepthBuffers();
}

void AQUA_NAMESPACE::FrontEndGraph::PrepareDepthBuffers()
{
	mConfig->mFramebufferFactory.Clear();
	mConfig->mFramebufferFactory.SetDepthAttribute("Depth", "D24UN_S8U");
	mConfig->mFramebufferFactory.SetDepthProperties(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);
	mConfig->mFramebufferFactory.SetTargetSize(mConfig->mShadowFeature.BaseResolution);

	auto error = mConfig->mFramebufferFactory.Validate();

	_STL_ASSERT(error, "Failed to validate depth factory");

	mConfig->mDepthBuffers.reserve(mConfig->mEnv->GetDirLightCount());

	for (const auto& lightSrc : mConfig->mEnv->GetDirLightSrcList())
	{
		auto depthBuffer = *mConfig->mFramebufferFactory.CreateFramebuffer();

		vkLib::ImageViewCreateInfo viewInfo{};
		viewInfo.Type = vk::ImageViewType::e2D;
		viewInfo.Format = vk::Format::eD24UnormS8Uint;
		viewInfo.ComponentMaps = { vk::ComponentSwizzle::eR };
		viewInfo.Subresource.aspectMask = vk::ImageAspectFlagBits::eDepth;
		viewInfo.Subresource.baseArrayLayer = 0;
		viewInfo.Subresource.baseMipLevel = 0;
		viewInfo.Subresource.layerCount = 1;
		viewInfo.Subresource.levelCount = 1;

		auto depthView = depthBuffer.GetDepthStencilAttachment()->CreateImageView(viewInfo);

		mConfig->mDepthBuffers.emplace_back(depthBuffer);
		mConfig->mDepthViews.push_back(depthView);
	}
}

void AQUA_NAMESPACE::FrontEndGraph::SetupShaders()
{
	mConfig->mDepthShader.SetFilepath("eVertex", (mConfig->mShaderDirectory / "Shadow.vert").string());
	mConfig->mDepthShader.SetFilepath("eFragment", (mConfig->mShaderDirectory / "Shadow.frag").string());

	mConfig->mDepthShader.CompileShaders();
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Wavefront AQUA_NAMESPACE::FrontEndGraph::GetOutputs() const
{
	return mConfig->mOutputs;
}

std::vector<vkLib::ImageView> AQUA_NAMESPACE::FrontEndGraph::GetDepthViews() const
{
	return mConfig->mDepthViews;
}

std::vector <vkLib::Framebuffer> AQUA_NAMESPACE::FrontEndGraph::GetDepthbuffers() const
{
	return mConfig->mDepthBuffers;
}
