#pragma once
#include "RendererConfig.h"
#include "RenderableSubmitInfo.h"
#include "Features.h"
#include "Environment.h"
#include "RenderableManagerie.h"

AQUA_BEGIN

struct RendererConfig;

// It's going to work like a #state_machine as almost everything else here
// combines both #forward and #deferred rendering separated by material instances
// not thread safe
class Renderer
{
public:
	AQUA_API Renderer();
	AQUA_API Renderer(vkLib::Context ctx, const std::filesystem::path& shaderDir);
	AQUA_API ~Renderer();

	AQUA_API void SetShaderDirectory(const std::filesystem::path& shaderDir);

	AQUA_API RendererFeatureFlags GetEnabledFeatures();

	AQUA_API void SetCtx(vkLib::Context ctx);
	AQUA_API void EnableFeatures(RendererFeatureFlags flags);
	AQUA_API void DisableFeatures(RendererFeatureFlags flags);

	AQUA_API void SetShadingbuffer(vkLib::Framebuffer framebuffer);
	AQUA_API void ResetRenderTarget(const glm::uvec2& resolution = { 1024, 1024 });

	AQUA_API void SetSSAOConfig(const SSAOFeature& config);
	AQUA_API void SetShadowConfig(const ShadowCascadeFeature& config);
	AQUA_API void SetBloomEffectConfig(const BloomEffectFeature& config);
	AQUA_API void SetEnvironment(EnvironmentRef env);
	AQUA_API void PrepareFeatures(); // first stage of preparation; setting up the renderer features and the environment

	// todo: shady behavior, be careful if you're calling it before shading network is prepared
	AQUA_API void InsertPreEventDependency(vkLib::Core::Ref<vk::Semaphore> signal);
	AQUA_API void InsertPostEventDependency(vkLib::Core::Ref<vk::Semaphore> signal,
		vk::PipelineStageFlags pipelineFlags = vk::PipelineStageFlagBits::eTopOfPipe);

	// independent of the renderer state
	AQUA_API void SetCamera(const glm::mat4& projection, const glm::mat4& view);

	AQUA_API void SubmitRenderable(const std::string& name, const glm::mat4& model, Renderable renderable, MaterialInstance instance);
	AQUA_API void SubmitRenderable(const std::string& name, const glm::mat4& model, const MeshData& mesh, MaterialInstance instance);
	// only accepts the defer material in the compute shader form
	AQUA_API void SubmitRenderable(const RenderableSubmitInfo& submitInfo);
	// Rendering different types of primitives
	// need a different kind of material to render lines
	// will help us in debugging and visualizing without setting up the renderables
	// it's a good idea to batch all line, curves and points together and submit them at once
	AQUA_API void SubmitLines(const std::string& lineIsland, const vk::ArrayProxy<Line>& lines, float thickness = 1.5f);
	AQUA_API void SubmitCurves(const std::string& curveIsland, const vk::ArrayProxy<Curve>& connections, float thickness = 1.5f);
	AQUA_API void SubmitBezierCurves(const std::string& curveIsland, const vk::ArrayProxy<Curve>& curves, float thickness = 1.5f);
	// we could even render points in the three space
	AQUA_API void SubmitPoints(const std::string& pointIsland, const vk::ArrayProxy<Point>& points, float pointSize = 1.5f);

	AQUA_API void RemoveRenderable(const std::string& name);
	AQUA_API void ClearRenderables();

	// todo: a little buggy; can't call this method more than once
	// it's designed to be called minimal amount of times but still
	// the user should be able to call it arbitrary number of times...
	AQUA_API void PrepareMaterialNetwork(); // passing the second stage

	// we're now in the active stage. We're free to select any renderable, upload memory to the GPU, and issue draw calls
	// any of the function below can be called at each frame without drastically impacting the performance

	AQUA_API SurfaceType GetSurfaceType(const std::string& name);
	AQUA_API MaterialType GetMaterialType(const std::string& name);

	AQUA_API void ActivateRenderables(const vk::ArrayProxy<std::string>& names);
	AQUA_API void ActivateAll();

	AQUA_API void DeactivateRenderables(const vk::ArrayProxy<std::string>& names);
	AQUA_API void DeactivateAll();

	// make sure the vertex factories match
	// can work per frame; requires another call to upload renderable with each
	// modification except for the model matrix
	AQUA_API void ModifyRenderable(const std::string& name, const MeshData& renderable);
	AQUA_API void ModifyRenderable(const std::string& name, Renderable renderable);
	AQUA_API void ModifyRenderable(const std::string& name, const vk::ArrayProxy<Line>& lines);
	AQUA_API void ModifyRenderable(const std::string& name, const vk::ArrayProxy<Curve>& curves);
	AQUA_API void ModifyRenderable(const std::string& name, const vk::ArrayProxy<Point>& points);
	AQUA_API void ModifyRenderable(const std::string& name, const glm::mat4& modelMatrix);

	// todo: fix me --> the workers have an internal conflict
	// we need to fix the revolution
	AQUA_API void InvalidateBuffers(); // uploading vertices to GPU; can be done each frame

	AQUA_API void UpdateDescriptors(); // updating the material and env descriptors

	// todo: the same problem occurring in the upload renderables
	// the working class starts a mutiny
	AQUA_API void IssueDrawCall(); // we're free to issue the draw call here

	// waits for the renderer to become idle
	AQUA_API vk::Result WaitIdle(std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

	// getters, only active after prepare features function
	AQUA_API vkLib::Framebuffer GetPostprocessbuffer() const;
	AQUA_API vkLib::Framebuffer GetShadingbuffer() const;

	AQUA_API VertexBindingMap GetVertexBindings() const;
	AQUA_API ImageAttributeList GetShaderInputAttributes() const;

	// for debugging...
	AQUA_API vkLib::Framebuffer GetGBuffer() const;
	AQUA_API vkLib::Framebuffer GetDepthbuffer() const;
	AQUA_API vkLib::ImageView GetDepthView() const;

private:
	SharedRef<RendererConfig> mConfig;

private:
	vk::Result WaitForWorker(uint32_t freeWorker, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

	void UpdateRenderableManagerie();

	Aqua::Exec::Graph PrepareShadingNetwork();
	void LinkShaderExecutionGraphs(EXEC_NAMESPACE::GenericDraft& draft, EXEC_NAMESPACE::NodeID forwardID, EXEC_NAMESPACE::NodeID deferID);


	void PrepareFramebuffers();
	void SetupFeatureInfos();

	void UpdateMaterialData();

	void SetupShaders();
};

AQUA_END
