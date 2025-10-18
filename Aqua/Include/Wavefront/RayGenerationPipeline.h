#pragma once
#include "RayTracingStructures.h"
#include "../Utils/CompilerErrorChecker.h"

AQUA_BEGIN
PH_BEGIN

// change required

struct RayGenerationPipeline : public vkLib::ComputePipeline
{
	RayGenerationPipeline() = default;
	RayGenerationPipeline(const vkLib::PShader& shader) { this->SetShader(shader); }

	AQUA_API void SetSceneInfo(const WavefrontSceneInfo& sceneInfo);
	AQUA_API void SetCamera(const PhysicalCamera& camera);

	AQUA_API virtual void UpdateDescriptors();

	RayBuffer GetRays() const { return mRays; }
	RayInfoBuffer GetRayInfos() const { return mRayInfos; }

	// Fields...
	//Buffers
	RayBuffer mRays;
	RayInfoBuffer mRayInfos;

	// Uniforms
	vkLib::Buffer<PhysicalCamera> mCamera;
	vkLib::Buffer<WavefrontSceneInfo> mSceneInfo;

	bool mCameraUpdated = false;
};

PH_END
AQUA_END
