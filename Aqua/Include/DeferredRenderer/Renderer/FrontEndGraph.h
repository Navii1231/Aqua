#pragma once
#include "Features.h"
#include "Environment.h"
#include "../Renderable/RenderTargetFactory.h"
#include "../../Execution/GraphBuilder.h"

AQUA_BEGIN

struct FrontEndGraphConfig;

class FrontEndGraph
{
private:
	FrontEndGraph();
	~FrontEndGraph() = default;

	void SetShaderDirectory(const std::filesystem::path& shaderDir);
	void SetCtx(vkLib::Context ctx);

	void SetEnvironment(EnvironmentRef env);
	void SetFeaturesInfos(const FeatureInfoMap& featureInfo);

	void SetFeatureFlags(RendererFeatureFlags flags);
	void SetShadowFeature(const ShadowCascadeFeature& feature);

	void SetVertexFactory(VertexFactory& factory);

	void PrepareFeatures();
	void PrepareDepthCascades();
	EXEC_NAMESPACE::Graph CreateGraph();

	void SetModels(Mat4Buf models);
	void PrepareFramebuffers(const glm::uvec2& rendererResolution);
	void PrepareDepthBuffers();

	void SetupShaders();

	std::vector<std::string> GetOutputs() const;

	std::vector<vkLib::ImageView> GetDepthViews() const;
	std::vector <vkLib::Framebuffer> GetDepthbuffers() const;

private:
	SharedRef<FrontEndGraphConfig> mConfig;

	friend class Renderer;
	friend struct RendererConfig;
};

AQUA_END
