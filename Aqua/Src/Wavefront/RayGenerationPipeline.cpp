#include "Core/Aqpch.h"
#include "Wavefront/RayGenerationPipeline.h"

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::RayGenerationPipeline::SetSceneInfo(const WavefrontSceneInfo& sceneInfo)
{
	mSceneInfo.Clear();
	mSceneInfo << sceneInfo;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::RayGenerationPipeline::SetCamera(const PhysicalCamera& camera)
{
	mCamera.Clear();
	mCamera << camera;
	mCameraUpdated = false;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::RayGenerationPipeline::UpdateDescriptors()
{
	vkLib::DescriptorWriter& writer = this->GetDescriptorWriter();

	vkLib::StorageBufferWriteInfo bufferInfo{};
	bufferInfo.Buffer = mRays.GetNativeHandles().Handle;

	writer.Update({ 0, 0, 0 }, bufferInfo);

	bufferInfo.Buffer = mRayInfos.GetNativeHandles().Handle;

	writer.Update({ 0, 4, 0 }, bufferInfo);

	vkLib::UniformBufferWriteInfo cameraInfo{};
	cameraInfo.Buffer = mCamera.GetNativeHandles().Handle;

	writer.Update({ 0, 1, 0 }, cameraInfo);

	vkLib::UniformBufferWriteInfo sceneInfo{};
	sceneInfo.Buffer = mSceneInfo.GetNativeHandles().Handle;

	writer.Update({ 1, 9, 0 }, sceneInfo);

	mCameraUpdated = true;
}
