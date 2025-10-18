#include "Core/Aqpch.h"
#include "Wavefront/TraceSession.h"

#include "Wavefront/BVHFactory.h"

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::Begin(const WavefrontTraceInfo& beginInfo)
{
	_STL_ASSERT(mSessionInfo->State == TraceSessionState::eReset ||
		mSessionInfo->State == TraceSessionState::eReady,
		"Begin method requires the WavefrontEstimator to be in either eReset or eReady state!");

	mSessionInfo->TraceInfo = beginInfo;
	mSessionInfo->CameraSpecs = beginInfo.CameraSpecs;

	mSessionInfo->SceneData.FrameCount = 1;
	mSessionInfo->SceneData.ResetImage = 1;
	mSessionInfo->SceneData.MeshCount = 0;
	mSessionInfo->SceneData.LightCount = 0;

	mSessionInfo->State = TraceSessionState::eOpenScope;

	Cleanup();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::SubmitRenderable(const MeshData& meshData, uint32_t bvhDepth)
{
	_STL_ASSERT(mSessionInfo->State == TraceSessionState::eOpenScope,
		"SubmitRenderable method requires the WavefrontEstimator to be in eOpenScope state!");

	auto bvhStruct = std::move(CreateBVH(meshData, bvhDepth));

	size_t NodeCount = mSessionInfo->LocalBuffers.Nodes.GetSize();

	CopyAllVertexAttribs(bvhStruct, meshData, RenderableType::eObject);

	MeshInfo meshInfo{};
	meshInfo.BeginIndex = static_cast<uint32_t>(NodeCount);
	meshInfo.EndIndex = static_cast<uint32_t>(mSessionInfo->LocalBuffers.Nodes.GetSize());

	mSessionInfo->MeshInfos << std::vector<MeshInfo>({ meshInfo });
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::SubmitLightSrc(const MeshData& meshData,
	const glm::vec3& lightIntensity, uint32_t bvhDepth)
{
	_STL_ASSERT(mSessionInfo->State == TraceSessionState::eOpenScope,
		"SubmitRenderable method requires the WavefrontEstimator to be in eOpenScope state!");

	auto bvhStruct = std::move(CreateBVH(meshData, bvhDepth));

	size_t NodeCount = mSessionInfo->LocalBuffers.Nodes.GetSize();

	CopyAllVertexAttribs(bvhStruct, meshData, RenderableType::eLightSrc);

	LightProperties props;
	props.Color = lightIntensity;

	mSessionInfo->LightPropsInfos << std::vector<LightProperties>({ props });

	LightInfo lightInfo{};
	lightInfo.BeginIndex = static_cast<uint32_t>(NodeCount);
	lightInfo.EndIndex = static_cast<uint32_t>(mSessionInfo->SharedBuffers.Nodes.GetSize());
	lightInfo.LightPropIndex = static_cast<uint32_t>(mSessionInfo->LightPropsInfos.GetSize() - 1);

	mSessionInfo->LightInfos << std::vector<LightInfo>({ lightInfo });
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::End()
{
	_STL_ASSERT(mSessionInfo->State == TraceSessionState::eOpenScope,
		"End method requires the WavefrontEstimator to be in eOpenScope state!");

	mSessionInfo->SceneData.MeshCount = static_cast<uint32_t>(mSessionInfo->MeshInfos.GetSize());
	mSessionInfo->SceneData.LightCount = static_cast<uint32_t>(mSessionInfo->LightInfos.GetSize());

	// TODO: Move the shared buffer into the local buffer data
	// For now, it has been done in submit functions...

	UpdateSceneBuffers();


	mSessionInfo->State = TraceSessionState::eReady;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::Clear()
{
	_STL_ASSERT(mSessionInfo->State == TraceSessionState::eReady || 
		mSessionInfo->State == TraceSessionState::eTracing,
		"::PH_FLUX_NAMESPACE::WavefrontEstimator::Clear requires the estimator to be in eTracing/eReady state!");

	mSessionInfo->State = TraceSessionState::eReady;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::Reset()
{
	_STL_ASSERT(mSessionInfo->State == TraceSessionState::eReady || 
		mSessionInfo->State == TraceSessionState::eReset ||
		mSessionInfo->State == TraceSessionState::eTracing,
		"Can't reset the WavefrontEstimator if the current state is at eReceiving!");

	mSessionInfo->State = TraceSessionState::eReset;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::SetCameraSpecs(const PhysicalCamera& camera)
{
	mSessionInfo->CameraSpecs = camera;

	if (mSessionInfo->State == TraceSessionState::eTracing)
		mSessionInfo->State = TraceSessionState::eReady;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::SetCameraView(const glm::mat4& view)
{
	mSessionInfo->CameraView = view;

	if (mSessionInfo->State == TraceSessionState::eTracing)
		mSessionInfo->State = TraceSessionState::eReady;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::Cleanup()
{
	mSessionInfo->ActiveBuffer = 0;

	mSessionInfo->LocalBuffers.Vertices.Clear();
	mSessionInfo->LocalBuffers.Faces.Clear();
	mSessionInfo->LocalBuffers.Normals.Clear();
	mSessionInfo->LocalBuffers.TexCoords.Clear();
	mSessionInfo->LocalBuffers.Nodes.Clear();

	mSessionInfo->SharedBuffers.Vertices.Clear();
	mSessionInfo->SharedBuffers.Faces.Clear();
	mSessionInfo->SharedBuffers.Normals.Clear();
	mSessionInfo->SharedBuffers.TexCoords.Clear();
	mSessionInfo->SharedBuffers.Nodes.Clear();

	mSessionInfo->MeshInfos.Clear();
	mSessionInfo->LightInfos.Clear();
	mSessionInfo->LightPropsInfos.Clear();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::UpdateSceneBuffers()
{
	mSessionInfo->CameraSpecsBuffer.Clear();
	mSessionInfo->CameraSpecsBuffer << mSessionInfo->CameraSpecs;
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVH AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::CreateBVH(
	const MeshData& meshData, uint32_t bvhDepth)
{
	// TODO: Here, we could use GPU to create BVH tree and store it ahead of time!
	BVHFactory bvhFactory;

	SplitStrategy strategy{};
	strategy.mSplit = BVHFactory::DefaultSplitFn::sSpatialSplit;

	bvhFactory.SetSplitStrategy(strategy);
	bvhFactory.SetDepth(bvhDepth);

	BVH bvhStruct = bvhFactory.Build(meshData.aPositions.begin(), meshData.aPositions.end(),
		meshData.aFaces.begin(), meshData.aFaces.end());

	return bvhStruct;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceSession::CopyAllVertexAttribs(BVH& bvhStruct,
	const MeshData& meshData, RenderableType renderableType)
{
	size_t VertexCount = mSessionInfo->LocalBuffers.Vertices.GetSize();
	size_t FaceCount = mSessionInfo->LocalBuffers.Faces.GetSize();
	size_t NodeCount = mSessionInfo->LocalBuffers.Nodes.GetSize();

	CopyVertexAttrib(mSessionInfo->SharedBuffers.Vertices, mSessionInfo->LocalBuffers.Vertices,
		bvhStruct.Vertices.begin(), bvhStruct.Vertices.end(),
		[](glm::vec4* BeginDevice, glm::vec4* EndDevice,
			glm::vec3* BeginHost, glm::vec3* EndHost)
	{
		while (BeginDevice != EndDevice)
		{
			*BeginDevice = glm::vec4(*BeginHost, 1.0f);

			BeginDevice++;
			BeginHost++;
		}
	});

	CopyVertexAttrib(mSessionInfo->SharedBuffers.Normals, mSessionInfo->LocalBuffers.Normals,
		meshData.aNormals.begin(), meshData.aNormals.end(),
		[](glm::vec4* BeginDevice, glm::vec4* EndDevice,
			const glm::vec3* BeginHost, const glm::vec3* EndHost)
	{
		while (BeginDevice != EndDevice)
		{
			*BeginDevice = glm::vec4(*BeginHost, 1.0f);

			BeginDevice++;
			BeginHost++;
		}
	});

	CopyVertexAttrib(mSessionInfo->SharedBuffers.TexCoords, mSessionInfo->LocalBuffers.TexCoords,
		meshData.aTexCoords.begin(), meshData.aTexCoords.end(),
		[](glm::vec2* BeginDevice, glm::vec2* EndDevice,
			const glm::vec3* BeginHost, const glm::vec3* EndHost)
	{
		while (BeginDevice != EndDevice)
		{
			*BeginDevice = glm::vec2(BeginHost->x, BeginHost->y);

			BeginDevice++;
			BeginHost++;
		}
	});

	CopyVertexAttrib(mSessionInfo->SharedBuffers.Faces, mSessionInfo->LocalBuffers.Faces,
		bvhStruct.Faces.begin(), bvhStruct.Faces.end(),
		[VertexCount, renderableType](Face* BeginDevice, Face* EndDevice,
			Face* BeginHost, Face* EndHost)
	{
		while (BeginDevice != EndDevice)
		{
			BeginDevice->Indices.x = BeginHost->Indices.x + static_cast<uint32_t>(VertexCount);
			BeginDevice->Indices.y = BeginHost->Indices.y + static_cast<uint32_t>(VertexCount);
			BeginDevice->Indices.z = BeginHost->Indices.z + static_cast<uint32_t>(VertexCount);

			BeginDevice->MaterialRef = BeginHost->MaterialRef;
			BeginDevice->FaceID = renderableType == RenderableType::eObject ? OBJECT_FACE_ID : LIGHT_FACE_ID;

			BeginDevice++;
			BeginHost++;
		}
	});

	CopyVertexAttrib(mSessionInfo->SharedBuffers.Nodes, mSessionInfo->LocalBuffers.Nodes,
		bvhStruct.Nodes.begin(), bvhStruct.Nodes.end(),
		[FaceCount, NodeCount](Node* BeginDevice, Node* EndDevice,
			Node* BeginHost, Node* EndHost)
	{
		while (BeginDevice != EndDevice)
		{
			Node& node = *BeginHost;

			node.BeginIndex += static_cast<uint32_t>(FaceCount);
			node.EndIndex += static_cast<uint32_t>(FaceCount);

			node.FirstChildIndex += static_cast<uint32_t>(NodeCount);
			node.SecondChildIndex += static_cast<uint32_t>(NodeCount);

			*BeginDevice = node;

			BeginDevice++;
			BeginHost++;
		}
	});
}
