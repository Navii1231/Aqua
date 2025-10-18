#pragma once
#include "RendererConfig.h"
#include "RenderableSubmitInfo.h"
#include "../Renderable/Renderable.h"
#include "../Renderable/RenderableBuilder.h"
#include "../../Material/MaterialInstance.h"
#include "../../Execution/GraphBuilder.h"

#include "../Renderable/BasicRenderables.h"
#include "Environment.h"
#include "MaterialSystem.h"
#include "BezierCurve.h"

AQUA_BEGIN

struct RenderableManagerieConfig;

class RenderableManagerie
{
public:

	void SubmitRenderable(const std::string& name, const glm::mat4& model, Renderable renderable, MaterialInstance instance);
	void SubmitRenderable(const std::string& name, const glm::mat4& model, const MeshData& mesh, MaterialInstance instance);
	// only accepts the defer material in the compute shader form
	void SubmitRenderable(const RenderableSubmitInfo& submitInfo);
	// Rendering different types of primitives
	// need a different kind of material to render lines
	// will help us in debugging and visualizing without setting up the renderables
	// it's a good idea to batch all line, curves and points together and submit them at once
	void SubmitLines(const std::string& lineIsland, const vk::ArrayProxy<Line>& lines, float thickness = 1.5f);
	void SubmitCurves(const std::string& curveIsland, const vk::ArrayProxy<Curve>& connections, float thickness = 1.5f);
	void SubmitBezierCurves(const std::string& curveIsland, const vk::ArrayProxy<Curve>& curves, float thickness = 1.5f);
	// we could even render points in the three space
	void SubmitPoints(const std::string& pointIsland, const vk::ArrayProxy<Point>& points, float pointSize = 1.5f);

	void RemoveRenderable(const std::string& name);
	void ClearRenderables();

	// make sure the vertex factories match
	// can work per frame; requires another call to upload renderable with each
	// modification except for the model matrix
	void ModifyRenderable(const std::string& name, const MeshData& renderable);
	void ModifyRenderable(const std::string& name, Renderable renderable);
	void ModifyRenderable(const std::string& name, const vk::ArrayProxy<Line>& lines);
	void ModifyRenderable(const std::string& name, const vk::ArrayProxy<Curve>& curves);
	void ModifyRenderable(const std::string& name, const vk::ArrayProxy<Point>& points);
	void ModifyRenderable(const std::string& name, const glm::mat4& modelMatrix);

	// renderable activation for vertex buffer drawing
	void ActivateRenderables(const vk::ArrayProxy<std::string>& names);
	void ActivateAll();

	void DeactivateRenderables(const vk::ArrayProxy<std::string>& names);
	void DeactivateAll();

	// uploading renderable to the main gpu buffer
	void InvalidateVertices();

	// getters
	Mat4Buf GetModels() const;
	VertexBindingMap GetVertexBindingInfo() const;

	std::vector<Core::MaterialInfo> GetDeferredMaterials() const;
	std::vector<Core::MaterialInfo> GetForwardMaterials() const;

private:
	SharedRef<RenderableManagerieConfig> mConfig;

	// comes from the renderer
	EnvironmentRef mEnv;
	vkLib::Framebuffer mShadingbuffer;

private:
	// born in the renderer and dies in the renderer
	RenderableManagerie();
	~RenderableManagerie();

	void SetCtx(vkLib::Context ctx);

	VertexFactory& GetVertexFactory() const;

	void SetManagerieData(EnvironmentRef ref, vkLib::Framebuffer shadingbuffer, MaterialSystem materialSystem);

	void TransferVertexRsc(VertexFactory& vertexFactory, const std::string& renderableName, uint32_t& vertexCount);

	void InsertModelMatrix(const glm::mat4& model);
	void StoreVertexMetaData(const RenderableSubmitInfo& submitInfo, uint32_t materialIdx);
	void EmplaceMaterialInfo(const RenderableSubmitInfo& submitInfo, EXEC_NAMESPACE::OpFn&& opFn, EXEC_NAMESPACE::OpUpdateFn&& updateFn);
	void AddMaterial(const RenderableSubmitInfo& submitInfo, uint32_t matIdx);


	void ExecuteLightingMaterial(vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op, const  RenderableSubmitInfo& instance, uint32_t matIdx);
	void ExecuteForwardMaterial(vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op, const RenderableSubmitInfo& instance, uint32_t matIdx);
	void UpdateLightingMaterial(EXEC_NAMESPACE::Operation& op, const RenderableSubmitInfo& submitInfo);
	void UpdateForwardMaterial(uint32_t matIdx, EXEC_NAMESPACE::Operation& op, const RenderableSubmitInfo& submitInfo);

	void SetupVertexBindings();
	void SetupVertexFactory();
	void SetupRenderableBuilder();


	vk::Result WaitForWorker(uint32_t freeWorker, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());
	vk::Result WaitIdle(std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());


	uint32_t CalculateActiveVertexCount();
	uint32_t CalculateActiveIndexCount();
	uint32_t CalculateVertexCount(const std::unordered_set<std::string>& activeRefs);
	uint32_t CalculateIndexCount(const std::unordered_set<std::string>& activeRefs);
	VertexFactory CreateVertexFactory(const VertexBindingMap& vertexInputs);

	friend class Renderer;
	friend struct RendererConfig;
};

AQUA_END
