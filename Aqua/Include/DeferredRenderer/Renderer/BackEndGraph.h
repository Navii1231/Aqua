#pragma once
#include "Features.h"
#include "../Renderable/RenderTargetFactory.h"
#include "../../Execution/GenericDraft.h"
#include "../../Execution/Ensemble.h"
#include "Environment.h"

AQUA_BEGIN

struct BackEndGraphConfig;

class BackEndGraph
{
private:
	BackEndGraph();
	~BackEndGraph() = default;

	void SetShaderDirectory(const std::filesystem::path& shaderDir);
	void SetCtx(vkLib::Context ctx);

	void SetEnvironment(EnvironmentRef env);

	void SetFeaturesInfos(const FeatureInfoMap& featureInfo);
	void SetFeatureFlags(RendererFeatureFlags flags);

	void PrepareFeatures();
	void PrepareSSAO();
	void PrepareBloomEffect();
	void PreparePostProcessing();
	EXEC_NAMESPACE::Graph CreateGraph();

	void SetModels(Mat4Buf models);

	void PrepareFramebuffers(const glm::uvec2& rendererResolution);
	void SetShadingFrameBuffer(vkLib::Framebuffer framebuffer);

	void SetupShaders();

	EXEC_NAMESPACE::Wavefront GetInputs() const;

	vkLib::Framebuffer GetPostprocessbuffer() const;

private:
	SharedRef<BackEndGraphConfig> mConfig;

	friend class Renderer;
	friend struct RendererConfig;
};

AQUA_END
