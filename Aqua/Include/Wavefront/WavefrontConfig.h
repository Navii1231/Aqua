#pragma once
#include "RayTracingStructures.h"

#include "SortRecorder.h"
#include "WavefrontWorkflow.h"
#include "RayGenerationPipeline.h"

#include "../Material/MaterialConfig.h"

AQUA_BEGIN
PH_BEGIN

#define OBJECT_FACE_ID    0
#define LIGHT_FACE_ID     1

#define OPTIMIZE_INTERSECTION   0

using RaySortRecorder = SortRecorder<uint32_t>;

enum class MaterialPreprocessState
{
	eSuccess               = 1,
	eInvalidSyntax         = 2,
	eShaderNotFound        = 3,
};

enum class TraceSessionState
{
	eReset             = 1,
	eOpenScope         = 2,
	eReady             = 3,
	eTracing           = 4,
};

struct MaterialShaderError
{
	std::string Info;
	MaterialPreprocessState State;
};

struct SessionInfo
{
	MeshInfoBuffer MeshInfos;
	LightInfoBuffer LightInfos;

	LightPropsBuffer LightPropsInfos;

	GeometryBuffers SharedBuffers;
	GeometryBuffers LocalBuffers;

	vkLib::Buffer<PhysicalCamera> CameraSpecsBuffer;
	vkLib::Buffer<ShaderData> ShaderConstData;

	WavefrontSceneInfo SceneData{};

	uint32_t ActiveBuffer = 0;

	glm::mat4 CameraView = glm::mat4(1.0f);
	PhysicalCamera CameraSpecs{};

	// For scoped operations...
	WavefrontTraceInfo TraceInfo{};

	TraceSessionState State = TraceSessionState::eReset;
};

struct WavefrontEstimatorCreateInfo
{
	vkLib::Context Context;
	std::string ShaderDirectory;

	// Work group sizes for pipelines
	glm::ivec2 RayGenWorkgroupSize = { 256, 256 };
	uint32_t IntersectionWorkgroupSize = 256;
	uint32_t MaterialEvalWorkgroupSize = 256;

	float Tolerance = 0.001f;
};

PH_END
AQUA_END
