#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/Environment.h"

void AQUA_NAMESPACE::Environment::ClearDirLights()
{
	mDirLightSrcs.clear();
	mDirCameras.clear();
}

void AQUA_NAMESPACE::Environment::ClearPointLights()
{
	mPointLightSrcs.clear();
	mPointCameras.clear();
}

void AQUA_NAMESPACE::Environment::SubmitLightSrc(const DirectionalLightInfo& src)
{
	src.Offset = static_cast<uint32_t>(mDirLightSrcs.size());
	mDirLightSrcs.emplace_back(src.SrcInfo);

	glm::vec3 movedFrame = src.CubeSize + src.Position;

	CameraInfo srcCamera{};
	srcCamera.Projection = glm::orthoLH(-movedFrame.x, movedFrame.x, movedFrame.y, -movedFrame.y, -movedFrame.z, movedFrame.z);
	srcCamera.View = glm::lookAtLH(src.Position, src.Position + glm::vec3(src.SrcInfo.Direction), glm::vec3(0.0f, 1.0f, 0.0f));

	mDirCameras.emplace_back(srcCamera);
}

void AQUA_NAMESPACE::Environment::SubmitLightSrc(const PointLightInfo& src)
{
	mPointLightSrcs.emplace_back(src.SrcInfo);

	// yet to implement the camera insertion

	mPointCameras.emplace_back();
}

void AQUA_NAMESPACE::Environment::RegenerateBuffers(vkLib::ResourcePool pool)
{
	if (mLightingBuffers.mDirCameraInfos)
		return;

	mLightingBuffers.mDirCameraInfos = pool.CreateBuffer<CameraInfo>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
	mLightingBuffers.mDirLightBuf = pool.CreateBuffer<DirectionalLightSrc>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
	mLightingBuffers.mPointCameraInfos = pool.CreateBuffer<CameraInfo>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
	mLightingBuffers.mPointLightBuf = pool.CreateBuffer<PointLightSrc>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
}

void AQUA_NAMESPACE::Environment::Update()
{
	mLightingBuffers.mDirCameraInfos.Clear();
	mLightingBuffers.mDirLightBuf.Clear();
	mLightingBuffers.mPointCameraInfos.Clear();
	mLightingBuffers.mPointLightBuf.Clear();

	mLightingBuffers.mDirCameraInfos << mDirCameras;
	mLightingBuffers.mDirLightBuf << mDirLightSrcs;
	mLightingBuffers.mPointCameraInfos << mPointCameras;
	mLightingBuffers.mPointLightBuf << mPointLightSrcs;
}
