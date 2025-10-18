#pragma once
#include "RendererConfig.h"

AQUA_BEGIN

struct LightingBuffers
{
	vkLib::Buffer<CameraInfo> mDirCameraInfos;
	vkLib::Buffer<CameraInfo> mPointCameraInfos;
	vkLib::Buffer<DirectionalLightSrc> mDirLightBuf;
	vkLib::Buffer<PointLightSrc> mPointLightBuf;
};

class Environment
{
public:
	Environment() = default;
	~Environment() = default;

	AQUA_API void ClearDirLights();
	AQUA_API void ClearPointLights();

	AQUA_API void SubmitLightSrc(const DirectionalLightInfo& src);
	AQUA_API void SubmitLightSrc(const PointLightInfo& src);

	void SubmitSkybox(vkLib::ImageView skyboxView) { mSkyboxImage = skyboxView; }
	void SetSkyboxSampler(vkLib::Core::Ref<vk::Sampler> sampler) { mSkyboxSampler = sampler; }

	size_t GetDirLightCount() const { return mDirLightSrcs.size(); }
	size_t GetPointLightCount() const { return mPointLightSrcs.size(); }

	const DirectionalLightList& GetDirLightSrcList() const { return mDirLightSrcs; }
	const PointLightList& GetPointLightSrcList() const { return mPointLightSrcs; }

	const std::vector<CameraInfo>& GetDirCameras() const { return mDirCameras; }
	const std::vector<CameraInfo>& GetPointCameras() const { return mPointCameras; }

	const LightingBuffers GetLightBuffers() const { return mLightingBuffers; }

	vkLib::ImageView GetSkybox() const { return mSkyboxImage; }
	vkLib::Core::Ref<vk::Sampler> GetSampler() const { return mSkyboxSampler; }

private:
	DirectionalLightList mDirLightSrcs;
	PointLightList mPointLightSrcs;

	std::vector<CameraInfo> mDirCameras;
	std::vector<CameraInfo> mPointCameras;

	vkLib::ImageView mSkyboxImage;
	vkLib::Core::Ref<vk::Sampler> mSkyboxSampler;

	// GPU buffer stuff
	LightingBuffers mLightingBuffers;

private:
	void RegenerateBuffers(vkLib::ResourcePool pool);
	void Update();

	friend class Renderer;
};

using EnvironmentRef = std::shared_ptr<Environment>;

AQUA_END
