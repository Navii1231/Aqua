#pragma once
#include "RayTracingStructures.h"
#include "../Utils/CompilerErrorChecker.h"

#include "MergeSorterPipeline.h"

AQUA_BEGIN
PH_BEGIN

using RayRef = typename MergeSorterPass<uint32_t>::ArrayRef;
using RayRefBuffer = vkLib::Buffer<RayRef>;

enum class RaySortEvent
{
	ePrepare                    = 1,
	eFinish                     = 2
};

enum class PostProcessFlagBits
{
	eToneMap                    = 1,
	eGammaCorrection            = 2,
	eGammaCorrectionInv         = 4,
};

using PostProcessFlags = vk::Flags<PostProcessFlagBits>;

struct IntersectionPipeline : public vkLib::ComputePipeline
{
	IntersectionPipeline() = default;
	IntersectionPipeline(const vkLib::PShader& shader) { this->SetShader(shader); }

	GeometryBuffers GetGeometryBuffers() const { return mGeometryBuffers; }

	AQUA_API void UpdateDescriptors();

// Fields...
	RayBuffer mRays;

	GeometryBuffers mGeometryBuffers;
	CollisionInfoBuffer mCollisionInfos;

	MeshInfoBuffer mMeshInfos;
	LightInfoBuffer mLightInfos;
	LightPropsBuffer mLightProps;

	vkLib::Buffer<WavefrontSceneInfo> mSceneInfo;

private:
	inline void UpdateGeometryBuffers();
};

struct RaySortEpiloguePipeline : public vkLib::ComputePipeline
{
	RaySortEpiloguePipeline() = default;
	RaySortEpiloguePipeline(const vkLib::PShader& shader) { this->SetShader(shader); }

	virtual void UpdateDescriptors();

// Fields...
	RayBuffer mRays;
	RayInfoBuffer mRaysInfos;
	CollisionInfoBuffer mCollisionInfos;

	RayRefBuffer mRayRefs;

	RaySortEvent mSortingEvent = RaySortEvent::ePrepare;

};

struct RayRefCounterPipeline : public vkLib::ComputePipeline
{
	RayRefCounterPipeline() = default;
	RayRefCounterPipeline(const vkLib::PShader& shader) { this->SetShader(shader); }
	
	void UpdateDescriptors();

// Fields...
	RayRefBuffer mRayRefs;
	vkLib::Buffer<uint32_t> mRefCounts;
};

// TODO: Make a proper Prefix summer
struct PrefixSumPipeline : public vkLib::ComputePipeline
{
	PrefixSumPipeline() = default;
	PrefixSumPipeline(const vkLib::PShader& shader) { this->SetShader(shader); }

	void UpdateDescriptors();

// Fields...
	vkLib::Buffer<uint32_t> mRefCounts;
};

struct LuminanceMeanPipeline : public vkLib::ComputePipeline
{
	LuminanceMeanPipeline() = default;
	LuminanceMeanPipeline(const vkLib::PShader& shader) { this->SetShader(shader); }

	void UpdateDescriptors();

	vkLib::Image mPixelMean;
	vkLib::Image mPixelVariance;
	vkLib::Image mPresentable;

	RayBuffer mRays;
	RayInfoBuffer mRayInfos;
	vkLib::Buffer<WavefrontSceneInfo> mSceneInfo;
};

struct PostProcessImagePipeline : public vkLib::ComputePipeline
{
	PostProcessImagePipeline() = default;
	PostProcessImagePipeline(const vkLib::PShader& shader) { this->SetShader(shader); }

	void UpdateDescriptors();

	vkLib::Image mPresentable;
};

PH_END
AQUA_END
