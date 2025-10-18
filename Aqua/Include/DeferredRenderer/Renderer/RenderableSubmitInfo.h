#pragma once
#include "RendererConfig.h"
#include "../Renderable/Renderable.h"
#include "../Renderable/VertexFactory.h"
#include "../../Material/MaterialInstance.h"

AQUA_BEGIN

struct RendererMaterialInstanceData
{
	// in case we've a forward pass, we need to specify which vertex input it takes
	// which vertex buffer resource the material needs from the renderer
	// the renderable vertex buffer name maps to index
	VertexBindingMap VertexInputs{};


	vkLib::DescriptorLocation mModelMatrixLocation;
	vkLib::DescriptorLocation mCameraLocation;

	// custom execution function for forward passes
	EXEC_NAMESPACE::OpFn mExecutionFn;
	EXEC_NAMESPACE::OpUpdateFn mUpdateFn = [](EXEC_NAMESPACE::Operation&) {}; // empty func by default

	bool mFeedingModelMatrices = false;
	bool mFeedingCameraInfo = false;

};

namespace Core
{
	// some important structs
	struct MaterialInfo
	{
		MAT_NAMESPACE::Material Op;
		SharedRef<MaterialInstanceInfo> Info;

		// these renderables will be drawn in the final draw call
		mutable std::unordered_set<std::string> mActiveRenderableRefs;
		std::unordered_set<std::string> mRenderableRefs;

		// each forward material will consist of its own vertex factory
		RendererMaterialInstanceData mData;
		VertexFactory mVertexFactory; // contains the vertices

		constexpr static uint64_t sMatTypeID = -1;
	};

	struct RenderableInfo
	{
		Renderable mInfo;
		size_t mMaterialRef = -1;
		bool mForwardMaterial = false;
	};

};

// maybe we'll set our update and execution function of the material
class RenderableSubmitInfo
{
public:
	RenderableSubmitInfo() = default;
	RenderableSubmitInfo(const std::string& name, const glm::mat4& modelMatrix,
		const Renderable& renderable, MaterialInstance material, const VertexBindingMap& bindingInputs)
		: Name(name), TransformMatrix(modelMatrix), GPURenderable3D(renderable)
	{ 
		mHasGPURenderable = true;
		Material = material;
		mMaterialData.VertexInputs = bindingInputs;

	}

	RenderableSubmitInfo(const std::string& name, const glm::mat4& modelMatrix,
		const MeshData& renderable, MaterialInstance material, const VertexBindingMap& bindingInputs)
		: Name(name), TransformMatrix(modelMatrix), CPURenderable3D(renderable)
	{ 
		mHasGPURenderable = false; 
		Material = material;
		mMaterialData.VertexInputs = bindingInputs;
	}

	void SetName(const std::string& name) { Name = name; }
	void SetTransform(const glm::mat4& model) { TransformMatrix = model; }
	void SetRenderableGPU(const Renderable& renderable) { GPURenderable3D = renderable; mHasGPURenderable = true; }
	void SetRenderableCPU(const MeshData& renderable) { CPURenderable3D = renderable; mHasGPURenderable = false; }

	void SetMaterial(const MaterialInstance& instance) { Material = instance; }
	void SetMaterialInputs(const VertexBindingMap& inputs) { mMaterialData.VertexInputs = inputs; }
	void SetMatExecuteFn(const EXEC_NAMESPACE::OpFn& fn) { mMaterialData.mExecutionFn = fn; }
	void SetMatUpdateFn(const EXEC_NAMESPACE::OpUpdateFn& fn) { mMaterialData.mUpdateFn = fn; }

	void SetModelMatrixDescLocation(const vkLib::DescriptorLocation& location, bool enable = true)
	{ mMaterialData.mModelMatrixLocation = location; mMaterialData.mFeedingModelMatrices = enable; }

	void SetCameraDescLocation(const vkLib::DescriptorLocation& location, bool enable = true)
	{ mMaterialData.mCameraLocation = location; mMaterialData.mFeedingCameraInfo = enable; }

	std::string GetName() const { return Name; }
	glm::mat4 GetMatrix() const { return TransformMatrix; }
	Renderable GetGPURenderable3D() const { return GPURenderable3D; }
	MeshData GetCPURenderable3D() const { return CPURenderable3D; }
	MaterialInstance GetMaterialInstance() const { return Material; }
	VertexBindingMap GetInputs() const { return mMaterialData.VertexInputs; }

	bool IsForwardPass() const 
	{ return Material.GetRendererPlatform() == MAT_NAMESPACE::Platform::eLightingRaster; }

private:
	std::string Name{};
	glm::mat4 TransformMatrix{};
	Renderable GPURenderable3D{};
	MeshData CPURenderable3D{};

	mutable RendererMaterialInstanceData mMaterialData;
	mutable MaterialInstance Material{}; // it could be forward or lighting pass
	
	bool mHasGPURenderable = false; // renderable memory could already be in a GPU or sitting in CPU

private:
	friend class Renderer;
	friend class RenderableManagerie;
};

AQUA_END
