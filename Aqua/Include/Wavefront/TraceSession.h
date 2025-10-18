#pragma once
#include "WavefrontConfig.h"

AQUA_BEGIN
PH_BEGIN

class TraceSession
{
public:
	TraceSession() = default;

	// Scoped operations...

	// Begin (eReset/eReady state --> eReceiving state)
	AQUA_API void Begin(const WavefrontTraceInfo& beginInfo);
	// (Only works at eReceiving stage)
	// (For developers: eLightSrc corresponds to face id -- 1 and eObject corresponds to 0)
	AQUA_API void SubmitRenderable(const MeshData& meshData, uint32_t bvhDepth);
	// (Only works at eReceiving stage)
	// (For developers: eLightSrc corresponds to face id -- 1 and eObject corresponds to 0)
	AQUA_API void SubmitLightSrc(const MeshData& meshData, const glm::vec3& lightIntensity, uint32_t bvhDepth);
	// Ending the scope (eReceiving state --> eReady state)
	AQUA_API void End();

	// To compute the iterations from the beginning again
	// (eReady/eTracing --> eReady state)
	AQUA_API void Clear();

	// Resets the Estimator state to ::PhFlux::WavefrontEstimatorState::eReset
	// (eReady/eTracing/eReset state --> eReset state)
	AQUA_API void Reset();

	// TODO: Maybe assert here as well if the path tracer is in the receiving state...
	AQUA_API void SetCameraSpecs(const PhysicalCamera& camera);
	AQUA_API void SetCameraView(const glm::mat4& view);

	// Getters...
	TraceSessionState GetState() const { return mSessionInfo->State; }

	GeometryBuffers GetLocalBuffers() const { return mSessionInfo->LocalBuffers; }
	vkLib::Buffer<PhysicalCamera> GetPhysicalCameraBuffer() const { return mSessionInfo->CameraSpecsBuffer; }

	const glm::mat4& GetCameraView() const { return mSessionInfo->CameraView; }
	const PhysicalCamera& GetCameraSpecs() const { return mSessionInfo->CameraSpecs; }

	explicit operator bool() const { return static_cast<bool>(mSessionInfo); }

private:
	std::shared_ptr<SessionInfo> mSessionInfo;

private:
	void Cleanup();

	void UpdateSceneBuffers();

	BVH CreateBVH(const MeshData& meshData, uint32_t bvhDepth);

	void CopyAllVertexAttribs(BVH& bvhStruct, const MeshData& meshData, RenderableType renderableType);

	template <typename T, typename Iter, typename Fn>
	void CopyVertexAttrib(vkLib::Buffer<T>& SharedBuffer, vkLib::Buffer<T>& LocalBuffer,
		Iter Begin, Iter End, Fn CopyRoutine);

	friend class WavefrontEstimator;
	friend class Executor;
};

template <typename T, typename Iter, typename Fn>
void PH_FLUX_NAMESPACE::TraceSession::CopyVertexAttrib(vkLib::Buffer<T>& SharedBuffer, 
	vkLib::Buffer<T>& LocalBuffer, Iter Begin, Iter End, Fn CopyRoutine)
{
	// TODO: CopyVertexAttrib calls vkLib::CopyBufferRegions internally
	// This function utilizes GPU to copy from shared buffers to locals
	// This is inherently slow! This process can be batched together for multiple meshes
	
	// The way you can do this is by first letting the shared buffer fill to certain max extent
	// And then immediately flush it out into the local buffer once it fills to that threshold

	size_t LocalBufferSize = LocalBuffer.GetSize();
	size_t HostCount = End - Begin;

	if (HostCount == 0)
		return;

	SharedBuffer.Clear();
	SharedBuffer.Resize(HostCount);

	T* BeginDevice = SharedBuffer.MapMemory(HostCount);
	T* EndDevice = BeginDevice + HostCount;

	CopyRoutine(BeginDevice, EndDevice, &(*Begin), &(*(End - 1)) + 1);

	SharedBuffer.UnmapMemory();

	LocalBuffer.Resize(LocalBufferSize + HostCount);

	vk::BufferCopy CopyInfo{};
	CopyInfo.setSrcOffset(0);
	CopyInfo.setDstOffset(LocalBufferSize);
	CopyInfo.setSize(HostCount);

	vkLib::CopyBufferRegions(LocalBuffer, SharedBuffer, std::vector<vk::BufferCopy>({ CopyInfo }));
}

PH_END
AQUA_END
