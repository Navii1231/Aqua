#pragma once
#include "WavefrontConfig.h"

AQUA_BEGIN
PH_BEGIN

/*
* user defined macro definitions...
* SHADER_TOLERENCE = 0.001, POWER_HEURISTIC_EXP = 2.0,
* EMPTY_MATERIAL_ID = -1, SKYBOX_MATERIAL_ID = -2, LIGHT_MATERIAL_ID = -3,
*/
struct MaterialCreateInfo
{
	std::string ShaderCode;
	uint32_t WorkGroupSize = 256;
	float ShadingTolerence = 0.001f;
	float IntersectionTolerence = 0.001f;
	float PowerHeuristics = 2.0f;
};

struct MaterialPipelineContext
{
public:
	MaterialPipelineContext() = default;

	MaterialCreateInfo mCreateInfo;

	// Set 0 stuff...
	RayBuffer mRays;
	RayInfoBuffer mRayInfos;
	CollisionInfoBuffer mCollisionInfos;
	GeometryBuffers mGeometry;
	LightInfoBuffer mLightInfos;
	LightPropsBuffer mLightProps;

	ShaderDataUniform mShaderData;

	// Other sets...
	std::unordered_map<vkLib::DescriptorLocation, vkLib::Core::BufferResource, MAT_NAMESPACE::DescriptorHasher> mBuffers;
	std::unordered_map<vkLib::DescriptorLocation, vkLib::Image, MAT_NAMESPACE::DescriptorHasher> mImages;
};

// TODO: Interface could be improved
class MaterialPipeline : public vkLib::ComputePipeline
{
public:
	MaterialPipeline() = default;
	MaterialPipeline(const vkLib::PShader& shader) { this->SetShader(shader); }

	template <typename T>
	void InsertBufferResource(const vkLib::DescriptorLocation& location, const vkLib::Buffer<T>& buffer);

	void InsertImageResource(const vkLib::DescriptorLocation& location, const vkLib::Image& image);

	AQUA_API void RemoveResource(const vkLib::DescriptorLocation& location);

	AQUA_API void UpdateDescriptors();

	void ClearBufferResources() { mHandle.mBuffers.clear(); }
	void ClearImageResources() { mHandle.mImages.clear(); }

private:
	MaterialPipelineContext mHandle;

	friend class WavefrontEstimator;
	friend class Executor;
};

template <typename T>
void PH_FLUX_NAMESPACE::MaterialPipeline::InsertBufferResource(
	const vkLib::DescriptorLocation& location, const vkLib::Buffer<T>& buffer)
{
	RemoveResource(location);

	mHandle.mBuffers[location] = buffer.GetBufferChunk();
}

PH_END
AQUA_END

