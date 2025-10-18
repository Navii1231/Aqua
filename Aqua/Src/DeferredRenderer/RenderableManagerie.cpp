#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/RenderableManagerie.h"
#include "DeferredRenderer/Renderable/CopyIndices.h"

AQUA_BEGIN

extern RenderableBuilder<ModelInfo>::CopyFnMap sVertexCopyFn;
extern RenderableBuilder<ModelInfo>::CopyFn sIndexCopyFn;

extern RenderableBuilder<HyperSurfInfo>::CopyFnMap sHyperSurfVertCopyFn;
extern RenderableBuilder<HyperSurfInfo>::CopyFn sHyperSurfIdxCopyFn;

struct RenderableManagerieConfig
{
	RenderableManagerieConfig() = default;
	~RenderableManagerieConfig() = default;

	// Material and Renderable stuff
	std::vector<Core::MaterialInfo> mMaterials;
	std::vector<Core::MaterialInfo> mForwardMaterials;

	std::unordered_map<std::string, Core::RenderableInfo> mRenderables;
	// SUGGESTION: we could use one giant buffer and map the ranges within the buffer for each renderable
	std::unordered_map<std::string, vkLib::GenericBuffer> mVertexMetaBuffers;

	// the things that will be rendered
	// TODO: this is kinda redundant since the active renderables are stored inside material info structs
	std::unordered_set<std::string> mActiveRenderables;

	// renderable stats
	uint32_t mRenderableCount = 0;
	uint32_t mReservedVertices = 1000;
	uint32_t mReservedIndices = 1000;
	VertexBindingMap mVertexBindingsInfo;
	VertexFactory mVertexFactory;

	// builder to take in the CPU renderables
	RenderableBuilder<ModelInfo> mRenderableBuilder;
	RenderableBuilder<HyperSurfInfo> mHyperSurfBuilder;

	// external dependencies; come from the renderer
	Mat4Buf mModels;
	MaterialSystem mMaterialSystem;

	vkLib::ResourcePool mResourcePool;
	vkLib::CommandBufferAllocator mCmdAlloc;
	vkLib::PipelineBuilder mPipelineBuilder;

	vkLib::Context mCtx;

	std::vector<vk::CommandBuffer> mCmds;
	std::vector<vkLib::Core::Worker> mWorkers;
	std::vector<CopyIdxPipeline> mCopyIndices;

	constexpr static uint64_t sMatTypeID = Core::MaterialInfo::sMatTypeID;
};

std::vector<Core::MaterialInfo>::const_iterator FindMaterialInstance(
	const RenderableSubmitInfo& submitInfo, const std::vector<Core::MaterialInfo>& materials)
{
	return std::find_if(materials.begin(), materials.end(),
		[&submitInfo](const Core::MaterialInfo& materialInfo)
	{

		SharedRef<vkLib::BasicPipeline> pipeline;
		SharedRef<vkLib::BasicPipeline> existingPipeline;

		// forward rendering
		if (materialInfo.Op.GFX)
		{
			pipeline = materialInfo.Op.GFX;
			existingPipeline = submitInfo.GetMaterialInstance().GetMaterial().GFX;
		}
		else // deferred rendering
		{
			pipeline = materialInfo.Op.Cmp;
			existingPipeline = submitInfo.GetMaterialInstance().GetMaterial().Cmp;
		}

		if (pipeline == existingPipeline && materialInfo.Info == submitInfo.GetMaterialInstance().GetInfo())
			return true;

		return false;
	});
}

AQUA_END

void AQUA_NAMESPACE::RenderableManagerie::SubmitRenderable(const std::string& name, const glm::mat4& model,
	Renderable renderable, MaterialInstance instance)
{
	RenderableSubmitInfo submitInfo(name, model, renderable, instance, {});
	SubmitRenderable(submitInfo);
}

void AQUA_NAMESPACE::RenderableManagerie::SubmitRenderable(const std::string& name, const glm::mat4& model, const MeshData& mesh, MaterialInstance instance)
{
	RenderableSubmitInfo submitInfo(name, model, mesh, instance, {});
	SubmitRenderable(submitInfo);
}

void AQUA_NAMESPACE::RenderableManagerie::SubmitRenderable(const RenderableSubmitInfo& submitInfo)
{
	auto rendererPlatform = submitInfo.Material.GetRendererPlatform();

	Renderable renderable = submitInfo.GPURenderable3D;

	// checking if we the mesh is in the CPU or GPU memory
	if (!submitInfo.mHasGPURenderable)
	{
		ModelInfo info{};
		info.CPUModelData = submitInfo.CPURenderable3D;

		renderable = mConfig->mRenderableBuilder.CreateRenderable(info);
	}

	mConfig->mRenderables[submitInfo.Name].mInfo = renderable;
	auto& materials = rendererPlatform == MAT_NAMESPACE::Platform::eLightingRaster ?
		mConfig->mForwardMaterials : mConfig->mMaterials;

	auto found = FindMaterialInstance(submitInfo, materials);

	uint32_t matIdx = found == materials.end() ?
		static_cast<uint32_t>(materials.size()) :
		static_cast<uint32_t>(found - materials.begin());

	// todo: could be avoided if the user doesn't provide meta data buffer
	StoreVertexMetaData(submitInfo, matIdx);
	InsertModelMatrix(submitInfo.TransformMatrix);

	if (found == materials.end())
		AddMaterial(submitInfo, matIdx);

	bool forward = submitInfo.Material.GetRendererPlatform() == MAT_NAMESPACE::Platform::eLightingRaster;

	mConfig->mRenderables[submitInfo.Name].mMaterialRef = matIdx;
	mConfig->mRenderables[submitInfo.Name].mForwardMaterial = forward;

	materials[matIdx].mRenderableRefs.insert(submitInfo.Name);
}

void AQUA_NAMESPACE::RenderableManagerie::SubmitLines(const std::string& lineIsland,
	const vk::ArrayProxy<Line>& lines, float thickness /*= 2.0f*/)
{
	HyperSurfInfo renderableInfo{};
	renderableInfo.Lines.insert(renderableInfo.Lines.begin(), lines.begin(), lines.end());
	renderableInfo.RepresentingLines = true;

	Renderable lineRenderable = mConfig->mHyperSurfBuilder.CreateRenderable(renderableInfo);
	MaterialInstance lineMaterial = *mConfig->mMaterialSystem[TEMPLATE_LINE];

	VertexBindingMap bindings{};
	bindings[0].SetName(ENTRY_LINE_VERTEX);
	bindings[0].AddAttribute(0, "RGBA32F");
	bindings[0].AddAttribute(1, "RGBA32F");

	RenderableSubmitInfo submission(lineIsland, glm::mat4(1.0f), lineRenderable, lineMaterial, bindings);

	submission.SetCameraDescLocation({ 0, 0, 0 });
	submission.SetMatUpdateFn([thickness](EXEC_NAMESPACE::Operation& op)
	{
		// setting the lines width and the rest will be taken care by the default update functionality
		op.GFX->SetLineWidth(thickness);
	});

	submission.SetMatExecuteFn([](vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op)
	{
		auto pipeline = *op.GFX;

		pipeline.Begin(buffer);

		pipeline.Activate();
		PushConst(pipeline, "eVertex.ShaderConstants.Index_0", 1.0f);

		pipeline.DrawIndexed(0, 0, 0, 1);

		pipeline.End();
	});

	SubmitRenderable(submission);
}

void AQUA_NAMESPACE::RenderableManagerie::SubmitCurves(const std::string& curveIsland,
	const vk::ArrayProxy<Curve>& connections, float thickness /*= 2.0f*/)
{
	HyperSurfInfo renderableInfo{};
	renderableInfo.Curves.insert(renderableInfo.Curves.begin(), connections.begin(), connections.end());
	renderableInfo.RepresentingLines = true;

	VertexBindingMap bindings{};
	bindings[0].SetName(ENTRY_LINE_VERTEX);
	bindings[0].AddAttribute(0, "RGBA32F");
	bindings[0].AddAttribute(1, "RGBA32F");

	Renderable lineRenderable = mConfig->mHyperSurfBuilder.CreateRenderable(renderableInfo);
	MaterialInstance lineMaterial = *mConfig->mMaterialSystem[TEMPLATE_LINE];

	RenderableSubmitInfo submission(curveIsland, glm::mat4(1.0f), lineRenderable, lineMaterial, bindings);

	submission.SetCameraDescLocation({ 0, 0, 0 });
	submission.SetMatUpdateFn([thickness](EXEC_NAMESPACE::Operation& op)
	{
		// setting the lines width and the rest will be taken care by the default update functionality
		op.GFX->SetLineWidth(thickness);
	});

	submission.SetMatExecuteFn([](vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op)
	{
		auto pipeline = *op.GFX;

		pipeline.Begin(buffer);

		pipeline.Activate();
		PushConst(pipeline, "eVertex.ShaderConstants.Index_0", 1.0f);

		pipeline.DrawIndexed(0, 0, 0, 1);

		pipeline.End();
	});

	SubmitRenderable(submission);
}

void AQUA_NAMESPACE::RenderableManagerie::SubmitBezierCurves(const std::string& curveIsland,
	const vk::ArrayProxy<Curve>& curves, float thickness /*= 2.0f*/)
{
	std::vector<Curve> basicCurves;
	basicCurves.reserve(curves.size());

	for (const auto& curve : curves)
	{
		BezierCurve curveSolver(curve);
		auto& thisCurve = basicCurves.emplace_back();

		thisCurve.Points = curveSolver.Solve();
		thisCurve.Color = curve.Color;
	}

	SubmitCurves(curveIsland, basicCurves, thickness);
}

void AQUA_NAMESPACE::RenderableManagerie::SubmitPoints(const std::string& pointIsland,
	const vk::ArrayProxy<Point>& points, float pointSize)
{
	HyperSurfInfo renderableInfo{};
	renderableInfo.Points.insert(renderableInfo.Points.begin(), points.begin(), points.end());
	renderableInfo.RepresentingLines = false;

	VertexBindingMap bindings{};
	bindings[0].SetName(ENTRY_POINT_VERTEX);
	bindings[0].AddAttribute(0, "RGBA32F");
	bindings[0].AddAttribute(1, "RGBA32F");

	Renderable pointRenderable = mConfig->mHyperSurfBuilder.CreateRenderable(renderableInfo);
	MaterialInstance pointMaterial = *mConfig->mMaterialSystem[TEMPLATE_POINT];
	uint32_t vertexCount = static_cast<uint32_t>(pointRenderable.GetVertexCount());

	RenderableSubmitInfo submitInfo(pointIsland, glm::mat4(1.0f), pointRenderable, pointMaterial, bindings);
	submitInfo.SetCameraDescLocation({ 0, 0, 0 });
	submitInfo.SetMatExecuteFn([pointSize, vertexCount](vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op)
	{
		auto pipeline = *op.GFX;

		pipeline.Begin(buffer);

		pipeline.Activate();
		PushConst(pipeline, "eVertex.ShaderConstants.Index_0", pointSize);

		pipeline.DrawVertices(0, 0, 1, vertexCount);

		pipeline.End();
	});

	SubmitRenderable(submitInfo);
}

void AQUA_NAMESPACE::RenderableManagerie::RemoveRenderable(const std::string& name)
{
	// find out the associated material and remove if it has no connections left
	size_t materialRef = mConfig->mRenderables.at(name).mMaterialRef;
	bool isForward = mConfig->mRenderables.at(name).mForwardMaterial;

	auto& materials = isForward ? mConfig->mForwardMaterials : mConfig->mMaterials;

	materials[materialRef].mRenderableRefs.erase(name);

	if (materials[materialRef].mActiveRenderableRefs.find(name) != materials[materialRef].mActiveRenderableRefs.end())
		materials[materialRef].mActiveRenderableRefs.erase(name);

	if (materials[materialRef].mRenderableRefs.size() == 0)
		materials.erase(materials.begin() + materialRef);

	// now remove the renderable
	mConfig->mRenderables.erase(name);
	mConfig->mVertexMetaBuffers.erase(name);

	if (std::find(mConfig->mActiveRenderables.begin(), mConfig->mActiveRenderables.end(), name) == mConfig->mActiveRenderables.end())
		mConfig->mActiveRenderables.erase(name);
}

void AQUA_NAMESPACE::RenderableManagerie::ClearRenderables()
{
	// Resetting every state relating GPU shading network memory
	mConfig->mRenderables.clear();
	mConfig->mMaterials.clear();
	mConfig->mActiveRenderables.clear();
	mConfig->mVertexMetaBuffers.clear();
	mConfig->mModels.Clear();
}

void AQUA_NAMESPACE::RenderableManagerie::ActivateRenderables(const vk::ArrayProxy<std::string>& names)
{
	for (const auto& name : names)
	{
		mConfig->mActiveRenderables.insert(name);

		const auto& renderableInfo = mConfig->mRenderables.at(name);

		if (renderableInfo.mForwardMaterial)
			mConfig->mForwardMaterials[renderableInfo.mMaterialRef].mActiveRenderableRefs.insert(name) = {};
		else
			mConfig->mMaterials[renderableInfo.mMaterialRef].mActiveRenderableRefs.insert(name) = {};
	}
}

void AQUA_NAMESPACE::RenderableManagerie::ActivateAll()
{
	std::vector<std::string> renderables;
	renderables.reserve(mConfig->mRenderables.size());

	for (const auto& [name, whatever] : mConfig->mRenderables)
		renderables.emplace_back(name);

	ActivateRenderables(renderables);
}

void AQUA_NAMESPACE::RenderableManagerie::DeactivateRenderables(const vk::ArrayProxy<std::string>& names)
{
	for (const auto& name : names)
	{
		// TODO: redundant operation?
		mConfig->mActiveRenderables.erase(name);

		const auto& renderableInfo = mConfig->mRenderables.at(name);

		if (renderableInfo.mForwardMaterial)
			mConfig->mForwardMaterials[renderableInfo.mMaterialRef].mActiveRenderableRefs.erase(name);
		else
			mConfig->mMaterials[renderableInfo.mMaterialRef].mActiveRenderableRefs.erase(name);
	}
}

void AQUA_NAMESPACE::RenderableManagerie::DeactivateAll()
{
	std::vector<std::string> renderables;
	renderables.reserve(mConfig->mRenderables.size());

	for (const auto& [name, whatever] : mConfig->mRenderables)
		renderables.emplace_back(name);

	DeactivateRenderables(renderables);
}

void AQUA_NAMESPACE::RenderableManagerie::ModifyRenderable(const std::string& name, const MeshData& renderable)
{
	auto gpu_renderable = mConfig->mRenderableBuilder.CreateRenderable({ renderable });
	ModifyRenderable(name, gpu_renderable);
}

void AQUA_NAMESPACE::RenderableManagerie::ModifyRenderable(const std::string& name, Renderable renderable)
{
	_STL_ASSERT(mConfig->mRenderables.find(name) != mConfig->mRenderables.end(),
		"renderable doesn't exist");

	auto& renderableInfo = mConfig->mRenderables[name];

	for (const auto& [name, vertexBuffers] : renderableInfo.mInfo.mVertexBuffers)
	{
		_STL_VERIFY(renderable.mVertexBuffers.find(name) != renderable.mVertexBuffers.end(),
			"vertex buffers don't match");
	}

	renderableInfo.mInfo = renderable;
}

void AQUA_NAMESPACE::RenderableManagerie::ModifyRenderable(const std::string& name, const vk::ArrayProxy<Line>& lines)
{
	_STL_ASSERT(mConfig->mRenderables.find(name) != mConfig->mRenderables.end(),
		"renderable doesn't exist");

	auto& renderableInfo = mConfig->mRenderables[name];

	_STL_ASSERT(renderableInfo.mInfo.mVertexBuffers.size() == 2, "vertices don't match");
	_STL_ASSERT(renderableInfo.mInfo.mVertexBuffers.find(ENTRY_LINE_VERTEX)
		!= renderableInfo.mInfo.mVertexBuffers.end(), "vertices don't match");

	HyperSurfInfo hyperSurfInfo{};
	hyperSurfInfo.Lines.insert(hyperSurfInfo.Lines.begin(), lines.begin(), lines.end());
	hyperSurfInfo.RepresentingLines = true;

	renderableInfo.mInfo = mConfig->mHyperSurfBuilder.CreateRenderable(hyperSurfInfo);
}

void AQUA_NAMESPACE::RenderableManagerie::ModifyRenderable(const std::string& name, const vk::ArrayProxy<Curve>& curves)
{
	_STL_ASSERT(mConfig->mRenderables.find(name) != mConfig->mRenderables.end(), "renderable doesn't exist");

	auto& renderableInfo = mConfig->mRenderables[name];

	_STL_ASSERT(renderableInfo.mInfo.mVertexBuffers.size() == 2, "vertices don't match");
	_STL_ASSERT(renderableInfo.mInfo.mVertexBuffers.find(ENTRY_LINE_VERTEX) != renderableInfo.mInfo.mVertexBuffers.end(), "vertices don't match");

	HyperSurfInfo hyperSurfInfo{};
	hyperSurfInfo.Curves.insert(hyperSurfInfo.Curves.begin(), curves.begin(), curves.end());
	hyperSurfInfo.RepresentingLines = true;

	renderableInfo.mInfo = mConfig->mHyperSurfBuilder.CreateRenderable(hyperSurfInfo);
}

void AQUA_NAMESPACE::RenderableManagerie::ModifyRenderable(const std::string& name, const vk::ArrayProxy<Point>& points)
{
	_STL_ASSERT(mConfig->mRenderables.find(name) != mConfig->mRenderables.end(),
		"renderable doesn't exist");

	auto& renderableInfo = mConfig->mRenderables[name];

	_STL_ASSERT(renderableInfo.mInfo.mVertexBuffers.size() == 1, "vertices don't match");
	_STL_ASSERT(renderableInfo.mInfo.mVertexBuffers.find(ENTRY_POINT_VERTEX)
		!= renderableInfo.mInfo.mVertexBuffers.end(), "vertices don't match");

	HyperSurfInfo hyperSurfInfo{};
	hyperSurfInfo.Points.insert(hyperSurfInfo.Points.begin(), points.begin(), points.end());
	hyperSurfInfo.RepresentingLines = false;

	renderableInfo.mInfo = mConfig->mHyperSurfBuilder.CreateRenderable(hyperSurfInfo);
}

void AQUA_NAMESPACE::RenderableManagerie::ModifyRenderable(const std::string& name, const glm::mat4& modelMatrix)
{
	_STL_ASSERT(mConfig->mRenderables.find(name) != mConfig->mRenderables.end(),
		"renderable doesn't exist");

	auto& renderableInfo = mConfig->mRenderables[name];

	auto modelMem = mConfig->mModels.MapMemory(1, renderableInfo.mInfo.VertexData.ModelIdx);
	*modelMem = modelMatrix;
	mConfig->mModels.UnmapMemory();
}

void AQUA_NAMESPACE::RenderableManagerie::InvalidateVertices()
{
	// Will be called per frame so it's supposed to work super fast
	// We'll be utilizing GPU to fill the vertex buffers of the renderer
	uint32_t vertexCount = 0;

	_STL_VERIFY(WaitIdle() == vk::Result::eSuccess, "couldn't wait for the renderer to become idle");

	mConfig->mVertexFactory.ClearBuffers();

	// first reserve the buffer space, and then transfer the data
	mConfig->mVertexFactory.ReserveVertices(CalculateActiveVertexCount());
	mConfig->mVertexFactory.ReserveIndices(CalculateActiveIndexCount());

	// first go through all the deferred materials
	for (auto& materialName : mConfig->mMaterials)
	{
		for (const auto& renderableName : materialName.mActiveRenderableRefs)
			TransferVertexRsc(mConfig->mVertexFactory, renderableName, vertexCount);
	}

	// doing some index mapping for each forward material
	for (auto& material : mConfig->mForwardMaterials)
	{
		material.mVertexFactory.ClearBuffers();

		// reserve enough space for the vertices and indices
		material.mVertexFactory.ReserveVertices(CalculateVertexCount(material.mActiveRenderableRefs));
		material.mVertexFactory.ReserveIndices(CalculateIndexCount(material.mActiveRenderableRefs));

		uint32_t fVertexCount = 0;

		for (auto& renderableName : material.mActiveRenderableRefs)
			TransferVertexRsc(material.mVertexFactory, renderableName, fVertexCount);
	}
}

AQUA_NAMESPACE::Mat4Buf AQUA_NAMESPACE::RenderableManagerie::GetModels() const
{
	return mConfig->mModels;
}

AQUA_NAMESPACE::VertexBindingMap AQUA_NAMESPACE::RenderableManagerie::GetVertexBindingInfo() const
{
	return mConfig->mVertexBindingsInfo;
}

std::vector<AQUA_NAMESPACE::Core::MaterialInfo> AQUA_NAMESPACE::RenderableManagerie::GetDeferredMaterials() const
{
	return mConfig->mMaterials;
}

std::vector<AQUA_NAMESPACE::Core::MaterialInfo> AQUA_NAMESPACE::RenderableManagerie::GetForwardMaterials() const
{
	return mConfig->mForwardMaterials;
}

AQUA_NAMESPACE::VertexFactory& AQUA_NAMESPACE::RenderableManagerie::GetVertexFactory() const
{
	return mConfig->mVertexFactory;
}

AQUA_NAMESPACE::RenderableManagerie::RenderableManagerie()
{
	mConfig = MakeRef<RenderableManagerieConfig>();

	SetupVertexBindings();
}

AQUA_NAMESPACE::RenderableManagerie::~RenderableManagerie()
{
	for (auto buf : mConfig->mCmds)
	{
		mConfig->mCmdAlloc.Free(buf);
	}
}

void AQUA_NAMESPACE::RenderableManagerie::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;

	mConfig->mPipelineBuilder = ctx.MakePipelineBuilder();
	mConfig->mResourcePool = ctx.CreateResourcePool();
	mConfig->mCmdAlloc = ctx.CreateCommandPools()[0];

	for (size_t i = 0; i < ctx.GetQueueCount(0); i++)
	{
		mConfig->mCmds.push_back(mConfig->mCmdAlloc.Allocate());
		mConfig->mCopyIndices.push_back(mConfig->mPipelineBuilder.BuildComputePipeline<CopyIdxPipeline>());

		mConfig->mWorkers.emplace_back(mConfig->mCtx.FetchWorker(0));
	}

	SetupVertexFactory();
	SetupRenderableBuilder();

	mConfig->mModels = mConfig->mResourcePool.CreateBuffer<glm::mat4>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent, 10);
}

void AQUA_NAMESPACE::RenderableManagerie::SetManagerieData(
	EnvironmentRef ref, vkLib::Framebuffer shadingbuffer, MaterialSystem materialSystem)
{
	mEnv = ref;
	mShadingbuffer = shadingbuffer;
	mConfig->mMaterialSystem = materialSystem;
}

void AQUA_NAMESPACE::RenderableManagerie::TransferVertexRsc(VertexFactory& vertexFactory, const std::string& renderableName, uint32_t& vertexCount)
{
	std::vector<uint32_t> history;

	uint32_t freeWorker = 0;

	auto renderable = mConfig->mRenderables[renderableName];
	auto vertexMetaBuf = mConfig->mVertexMetaBuffers[renderableName];
	auto cmd = mConfig->mCmds[freeWorker];

	_STL_VERIFY(WaitForWorker(freeWorker) == vk::Result::eSuccess, "Can't wait for the worker");

	cmd.reset();
	cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	auto& buffers = renderable.mInfo.mVertexBuffers;

	vertexFactory.TraverseBuffers([&buffers, cmd](uint32_t idx, const std::string& name, vkLib::GenericBuffer buffer)
	{
		// vertex meta infos are copied separately
		if (name == ENTRY_METADATA)
			return;

		// requires a one to one correspondence b/w vertex factory names and the renderable ref vertex buffer names
		// may a mismatch occurs, we'll have a runtime exception at the 'at' method
		VertexFactory::CopyVertexBuffer(cmd, buffer, buffers.at(name));
	});

	// vertex meta infos are copied separately
	if (vertexFactory.BindingExists(ENTRY_METADATA))
		VertexFactory::CopyVertexBuffer(cmd, vertexFactory[ENTRY_METADATA], vertexMetaBuf);

	mConfig->mCopyIndices[freeWorker](cmd, vertexFactory.GetIndexBuffer(),
		renderable.mInfo.mIndexBuffer, vertexCount);

	vertexCount += static_cast<uint32_t>(renderable.mInfo.GetVertexCount());

	cmd.end();

	// incrementing the counter to select the next worker
	auto submissionIdx = mConfig->mWorkers[freeWorker++].Dispatch(cmd);

	history.push_back(submissionIdx);
}

void AQUA_NAMESPACE::RenderableManagerie::InsertModelMatrix(const glm::mat4& model)
{
	// double the capacity if we hit the buffer limit
	if (mConfig->mModels.GetCapacity() < mConfig->mModels.GetSize() + 1)
		mConfig->mModels.Reserve(2 * mConfig->mModels.GetSize());

	mConfig->mModels << model;
}

void AQUA_NAMESPACE::RenderableManagerie::StoreVertexMetaData(const RenderableSubmitInfo& submitInfo, uint32_t materialIdx)
{
	/****************** Copying the model idx, material idx and the parameter set info *********************/
	auto& renderable = mConfig->mRenderables[submitInfo.Name].mInfo;
	VertexMetaData& vertexData = mConfig->mRenderables[submitInfo.Name].mInfo.VertexData;
	vertexData.ModelIdx = static_cast<uint32_t>(mConfig->mModels.GetSize());
	vertexData.MaterialIdx = materialIdx;
	vertexData.ParameterOffset = static_cast<uint32_t>(submitInfo.Material.GetOffset());

	vertexData.Stride = sizeof(glm::vec3);
	vertexData.Offset = 0;

	auto vertexDataBuffer = mConfig->mResourcePool.CreateGenericBuffer(
		renderable.Info.Usage, vk::MemoryPropertyFlagBits::eHostCoherent);

	Renderable::UpdateMetaData(vertexDataBuffer, vertexData, 0, static_cast<uint32_t>(renderable.GetVertexCount()));

	mConfig->mVertexMetaBuffers[submitInfo.Name] = vertexDataBuffer;
	/************************************************************************************************/
}

void AQUA_NAMESPACE::RenderableManagerie::EmplaceMaterialInfo(const RenderableSubmitInfo& submitInfo,
	EXEC_NAMESPACE::OpFn&& opFn, EXEC_NAMESPACE::OpUpdateFn&& updateFn)
{
	Core::MaterialInfo info{};
	info.Info = submitInfo.Material.GetInfo();
	info.Op = submitInfo.Material.GetMaterial();

	// make a copy to the underlying graphics pipeline
	switch (submitInfo.Material.GetRendererPlatform())
	{
		case MAT_NAMESPACE::Platform::eLightingRaster:
			info.Op.GFX = MakeRef(*submitInfo.Material.GetMaterial().GFX);
			break;
		case MAT_NAMESPACE::Platform::eLightingCompute:
			info.Op.Cmp = MakeRef(*submitInfo.Material.GetMaterial().Cmp);
			break;
		default:
			_STL_ASSERT(false, "invalid material instance");
			break;
	}

	// holding one instance of the material instance here
	info.Op.UpdateFn = updateFn;
	info.Op.Fn = opFn;

	if (submitInfo.Material.GetRendererPlatform() == MAT_NAMESPACE::Platform::eLightingRaster)
	{
		info.mVertexFactory = CreateVertexFactory(submitInfo.mMaterialData.VertexInputs);
		auto& material = mConfig->mForwardMaterials.emplace_back(info);
		material.mData = submitInfo.mMaterialData;
		return;
	}

	mConfig->mMaterials.push_back(info);
}

void AQUA_NAMESPACE::RenderableManagerie::AddMaterial(const RenderableSubmitInfo& submitInfo, uint32_t matIdx)
{
	bool forward = submitInfo.Material.GetRendererPlatform() == MAT_NAMESPACE::Platform::eLightingRaster;

	auto forwardMaterialExecute = [this, submitInfo, matIdx](vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op)
	{ ExecuteForwardMaterial(buffer, op, submitInfo, matIdx); };

	auto deferredMaterialExecute = [this, submitInfo, matIdx](vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op)
	{ ExecuteLightingMaterial(buffer, op, submitInfo, matIdx); };

	auto forwardMaterialUpdate = [this, submitInfo, matIdx](EXEC_NAMESPACE::Operation& op)
	{ UpdateForwardMaterial(matIdx, op, submitInfo); };

	auto deferredMaterialUpdate = [this, submitInfo, matIdx](EXEC_NAMESPACE::Operation& op)
	{ UpdateLightingMaterial(op, submitInfo); };

	if (forward)
		EmplaceMaterialInfo(submitInfo, forwardMaterialExecute, forwardMaterialUpdate);
	else
		EmplaceMaterialInfo(submitInfo, deferredMaterialExecute, deferredMaterialUpdate);
}

void AQUA_NAMESPACE::RenderableManagerie::ExecuteLightingMaterial(vk::CommandBuffer buffer,
	const EXEC_NAMESPACE::Operation& op, const RenderableSubmitInfo& submitInfo, uint32_t matIdx)
{
	// renderer only accepts compute shader as the deferred materials
	EXEC_NAMESPACE::CBScope executioner(buffer);

	auto& pipeline = *op.Cmp;
	auto& instance = submitInfo.Material;

	glm::uvec2 resolution = mShadingbuffer.GetResolution();
	glm::uvec3 workGroupSize = pipeline.GetWorkGroupSize();
	glm::uvec3 workGroups = { resolution.x / workGroupSize.x + 1, resolution.y / workGroupSize.y + 1, 1 };

	pipeline.Begin(buffer);

	instance.TraverseResources([buffer](const vkLib::DescriptorLocation& descInfo, MAT_NAMESPACE::Resource& resource)
	{
		if (resource.Type != MaterialInstance::GetSampledImageDescType() &&
			resource.Type != MaterialInstance::GetStorageImageDescType())
			return;

		resource.ImageView->BeginCommands(buffer);
		resource.ImageView->RecordTransitionLayout(vk::ImageLayout::eGeneral);
	});

	pipeline.Activate();

	PushConst(pipeline, "eCompute.ShaderConstants.Index_0", resolution);
	PushConst(pipeline, "eCompute.ShaderConstants.Index_1", static_cast<uint32_t>(mEnv->GetDirLightCount()));
	PushConst(pipeline, "eCompute.ShaderConstants.Index_2", static_cast<uint32_t>(mEnv->GetPointLightCount()));
	PushConst(pipeline, "eCompute.ShaderConstants.Index_3", matIdx);

	pipeline.Dispatch(workGroups);

	instance.TraverseResources([buffer](const vkLib::DescriptorLocation& descInfo, MAT_NAMESPACE::Resource& resource)
	{
		if (resource.Type != MaterialInstance::GetSampledImageDescType() &&
			resource.Type != MaterialInstance::GetStorageImageDescType())
			return;

		resource.ImageView->EndCommands();
	});

	pipeline.End();
}

void AQUA_NAMESPACE::RenderableManagerie::ExecuteForwardMaterial(vk::CommandBuffer buffer,
	const EXEC_NAMESPACE::Operation& op, const RenderableSubmitInfo& submitInfo, uint32_t matIdx)
{
	EXEC_NAMESPACE::CBScope executioner(buffer);

	auto& hsLinePipeline = *op.GFX;

	hsLinePipeline.SetFramebuffer(mShadingbuffer);

	if (submitInfo.mMaterialData.mExecutionFn)
	{
		submitInfo.mMaterialData.mExecutionFn(buffer, op);
		return;
	}
	hsLinePipeline.Begin(buffer);
	hsLinePipeline.Activate();

	// we need a to inform this functions that not all vertices in the buffer represent lines
	hsLinePipeline.DrawIndexed(0, 0, 0, 1);

	hsLinePipeline.End();
}

void AQUA_NAMESPACE::RenderableManagerie::UpdateLightingMaterial(EXEC_NAMESPACE::Operation& op, const RenderableSubmitInfo& submitInfo)
{
	// note: this implementation of renderer only accepts compute pipelines as lighting passes
	auto resolution = mShadingbuffer.GetResolution();

	auto& pipeline = *op.Cmp;

	submitInfo.mMaterialData.mUpdateFn(op);

	submitInfo.Material.UpdateDescriptors();
}

void AQUA_NAMESPACE::RenderableManagerie::UpdateForwardMaterial(uint32_t matIdx, EXEC_NAMESPACE::Operation& op, const RenderableSubmitInfo& submitInfo)
{
	auto resolution = mShadingbuffer.GetResolution();
	auto& materialInfo = mConfig->mForwardMaterials[matIdx];
	auto& pipeline = *op.GFX;

	pipeline.SetClearColorValues(0, { 0.0f, 1.0f, 0.0f, 1.0f });
	pipeline.SetClearDepthStencilValues(1.0f, 0);

	pipeline.SetScissor(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(resolution.x, resolution.y)));
	pipeline.SetViewport(vk::Viewport(0.0f, 0.0f, (float)resolution.x, (float)resolution.y, 0.0f, 1.0f));

	// setting all the vertex and index buffers into the forward rendering materials
	for (const auto& [idx, binding] : submitInfo.mMaterialData.VertexInputs)
	{
		pipeline.SetVertexBuffer(idx, materialInfo.mVertexFactory[binding.Name]);
	}

	pipeline.SetIndexBuffer(materialInfo.mVertexFactory.GetIndexBuffer());

	submitInfo.mMaterialData.mUpdateFn(op);

	submitInfo.Material.UpdateDescriptors();
}

void AQUA_NAMESPACE::RenderableManagerie::SetupVertexBindings()
{
	// TODO: can be customized later; for now we shall go with these vertex attributes only
	auto& vertexBindings = mConfig->mVertexBindingsInfo;

	vertexBindings.clear();
	vertexBindings[0].SetName(ENTRY_POSITION);
	vertexBindings[0].AddAttribute(0, "RGB32F");

	vertexBindings[1].SetName(ENTRY_NORMAL);
	vertexBindings[1].AddAttribute(1, "RGB32F");

	vertexBindings[2].SetName(ENTRY_TANGENT_SPACE);
	vertexBindings[2].AddAttribute(2, "RGB32F");
	vertexBindings[2].AddAttribute(3, "RGB32F");

	vertexBindings[3].SetName(ENTRY_TEXCOORDS);
	vertexBindings[3].AddAttribute(4, "RGB32F");

	vertexBindings[4].SetName(ENTRY_METADATA);
	vertexBindings[4].AddAttribute(5, "RGB32F");
}

void AQUA_NAMESPACE::RenderableManagerie::SetupVertexFactory()
{
	auto& vertexFac = mConfig->mVertexFactory;

	vertexFac.ClearBuffers();
	vertexFac.Reset();

	vertexFac.SetResourcePool(mConfig->mResourcePool);
	vertexFac.SetVertexBindings(mConfig->mVertexBindingsInfo);

#if _DEBUG
	vertexFac.SetAllVertexProperties(vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eHostCoherent);
#else
	vertexFac.SetAllVertexProperties(vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal);
#endif

#if _DEBUG
	vertexFac.SetIndexProperties(vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eHostCoherent);
#else
	vertexFac.SetIndexProperties(vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal);
#endif

	auto error = vertexFac.Validate();
	_STL_ASSERT(error, "Couldn't setup the vertex factory in the renderer");

	vertexFac.ReserveVertices(mConfig->mReservedVertices);
	vertexFac.ReserveIndices(mConfig->mReservedIndices);
}

void AQUA_NAMESPACE::RenderableManagerie::SetupRenderableBuilder()
{
	mConfig->mRenderableBuilder.SetRscPool(mConfig->mResourcePool);
	mConfig->mHyperSurfBuilder.SetRscPool(mConfig->mResourcePool);

	mConfig->mRenderableBuilder.SetIndexProperties(sIndexCopyFn, vk::BufferUsageFlagBits::eStorageBuffer);
	mConfig->mHyperSurfBuilder.SetIndexProperties(sHyperSurfIdxCopyFn, vk::BufferUsageFlagBits::eStorageBuffer);

	for (const auto& [name, fn] : sVertexCopyFn)
	{
		mConfig->mRenderableBuilder.SetVertexProperties(name, fn, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	for (const auto& [name, fn] : sHyperSurfVertCopyFn)
	{
		mConfig->mHyperSurfBuilder.SetVertexProperties(name, fn, vk::BufferUsageFlagBits::eStorageBuffer);
	}
}

vk::Result AQUA_NAMESPACE::RenderableManagerie::WaitForWorker(uint32_t freeWorker, std::chrono::nanoseconds timeout /*= std::chrono::nanoseconds::max()*/)
{
	return mConfig->mWorkers[freeWorker].WaitIdle(timeout);
}

vk::Result AQUA_NAMESPACE::RenderableManagerie::WaitIdle(std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	std::vector<vk::Fence> fences;
	fences.reserve(mConfig->mWorkers.size() + 1);

	for (const auto& worker : mConfig->mWorkers)
	{
		fences.emplace_back(worker.GetFence());
	}

	//fences.emplace_back(mConfig->mRendererWorker.GetFence());

	return mConfig->mCtx.WaitForFences(fences, true, timeOut);
}

uint32_t AQUA_NAMESPACE::RenderableManagerie::CalculateActiveVertexCount()
{
	uint32_t vertexCount = 0;

	for (const auto& material : mConfig->mMaterials)
	{
		for (const auto& name : material.mActiveRenderableRefs)
		{
			vertexCount += static_cast<uint32_t>(mConfig->mRenderables[name].mInfo.GetVertexCount());
		}
	}

	return vertexCount;
}

uint32_t AQUA_NAMESPACE::RenderableManagerie::CalculateActiveIndexCount()
{
	uint32_t indexCount = 0;

	for (const auto& material : mConfig->mMaterials)
	{
		for (const auto& name : material.mActiveRenderableRefs)
		{
			indexCount += static_cast<uint32_t>(mConfig->mRenderables[name].mInfo.GetIndexCount());
		}
	}

	return indexCount;
}

uint32_t AQUA_NAMESPACE::RenderableManagerie::CalculateVertexCount(const std::unordered_set<std::string>& activeRefs)
{
	uint32_t vertexCount = 0;

	for (const auto& name : activeRefs)
	{
		vertexCount += static_cast<uint32_t>(mConfig->mRenderables[name].mInfo.GetVertexCount());
	}

	return vertexCount;
}

uint32_t AQUA_NAMESPACE::RenderableManagerie::CalculateIndexCount(const std::unordered_set<std::string>& activeRefs)
{
	uint32_t indexCount = 0;

	for (const auto& name : activeRefs)
	{
		indexCount += static_cast<uint32_t>(mConfig->mRenderables[name].mInfo.GetIndexCount());
	}

	return indexCount;
}

AQUA_NAMESPACE::VertexFactory AQUA_NAMESPACE::RenderableManagerie::CreateVertexFactory(const VertexBindingMap& vertexInputs)
{
	VertexFactory fac{};
	fac.SetResourcePool(mConfig->mResourcePool);
	fac.SetVertexBindings(vertexInputs);
#if _DEBUG
	fac.SetAllVertexProperties(vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eHostCoherent);
#else
	fac.SetAllVertexProperties(vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal);
#endif

#if _DEBUG
	fac.SetIndexProperties(vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eHostCoherent);
#else
	fac.SetIndexProperties(vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal);
#endif

	_STL_VERIFY(fac.Validate(), "couldn't create vertex factory");

	return fac;
}
