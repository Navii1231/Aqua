#pragma once
#include "WavefrontConfig.h"
#include "MaterialPipeline.h"
#include "../Material/MaterialInstance.h"

#include "TraceSession.h"

AQUA_BEGIN
PH_BEGIN

using MaterialPipelineList = std::vector<MaterialInstance>;

struct ExecutionPipelines
{
	RayGenerationPipeline RayGenerator; // Simulates physical camera...
	IntersectionPipeline IntersectionPipeline; // Intersection testing stage...

	// Sorting stages...
	RaySortEpiloguePipeline RaySortPreparer;
	std::shared_ptr<RaySortRecorder> SortRecorder;
	RaySortEpiloguePipeline RaySortFinisher;

	// Ref counting and prefix sum stages...
	RayRefCounterPipeline RayRefCounter;
	PrefixSumPipeline PrefixSummer; // Fix me: bad implementation... 

	// Handles three default shaders: Empty, Skybox, and light shader
	// All of them will deactivate the ray
	MaterialInstance InactiveRayShader; // TODO: Skybox shader hasn't been implemented yet...

	LuminanceMeanPipeline LuminanceMean; // Accumulates the incoming light into an average sum
	PostProcessImagePipeline PostProcessor; // For post processing...
};

struct ExecutionBlock
{
	uint32_t mBounceIdx = 0;
	uint32_t mActiveBuffer = 0;
};

struct ExecutorCreateInfo
{
	glm::ivec2 TargetResolution = { 1920, 1080 };
	glm::ivec2 TileSize = { 1920, 1080 };

	vk::BufferUsageFlags BufferUsage{};
	vk::MemoryPropertyFlags MemoryProps = vk::MemoryPropertyFlagBits::eDeviceLocal;

	bool AllowSorting = true;
};

struct ExecutionInfo
{
	// Pipeline resources...
	ExecutionPipelines PipelineResources;
	MaterialPipelineList MaterialResources;

	TraceSession TracingSession;

	// Set by the WavefrontEstimator class
	RayBuffer Rays;
	RayInfoBuffer RayInfos;
	CollisionInfoBuffer CollisionInfos;
	RayRefBuffer RayRefs; // For sorting...

	vkLib::Buffer<uint32_t> RefCounts; // Resized by the SetMaterialPipelines
	vkLib::Buffer<WavefrontSceneInfo> Scene;

	// Target images...
	EstimatorTarget Target{};

	// Creation Info
	ExecutorCreateInfo CreateInfo{};
	WavefrontTraceInfo TracingInfo{};

	// execution stuff
	std::vector<vkLib::Core::Worker> Workers;
	vkLib::CommandBufferAllocator CmdAlloc;

	// Random stuff...
	std::uniform_int_distribution<uint32_t> UniformDistribution;

	std::random_device RandomDevice;
	std::mt19937 RandomEngine;

	// Init random stuff...
	ExecutionInfo()
		: RandomDevice(), RandomEngine(RandomDevice()) {}
};

PH_END
AQUA_END
