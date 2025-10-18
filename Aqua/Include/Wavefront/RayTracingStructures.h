#pragma once
#include "../Geometry3D/GeometryConfig.h"

#include "Core.h"

AQUA_BEGIN
PH_BEGIN

enum class CameraMovementFlagBits
{
	eNone                = 0,
	eForward             = 1,
	eBackward            = 2,
	eRight               = 4,
	eLeft                = 8,
	eUp                  = 16,
	eDown                = 32,
};

enum class RenderableType
{
	eObject              = 1,
	eLightSrc            = 2,
};

enum class TraceResult
{
	eUndefined           = 0,
	ePending             = 1,
	eTraversing          = 2,
	eComplete            = 3,
	eUnknownError        = 4,
};

using CameraMovementFlags = vk::Flags<CameraMovementFlagBits>;

struct Ray
{
	alignas(16) glm::vec3 Origin;
	alignas(4)  uint32_t MaterialIndex;
	alignas(16) glm::vec3 Direction;
	alignas(4)  uint32_t Active;
};

struct RayInfo
{
	alignas(16) glm::uvec2 ImageCoordinate;
	alignas(16) glm::vec4 Luminance;
	alignas(16) glm::vec4 Throughput;
};

// Set 1

struct PhysicalCamera
{
	alignas(8) glm::vec2 SensorSize = glm::vec2(36.0f, 24.0f); // in mm
	alignas(4) float FocalLength = 50.0f; // in mm
	alignas(4) float ApertureSize = 2.8f; // in mm
	alignas(4) float FocalDistance = 1.0f; // in meters
	alignas(4) float FOV = 90.0f; // in degrees
};

struct Camera
{
	alignas(16) glm::vec3 Position = glm::vec3(0.0f, 0.0f, 0.0f);
	alignas(16) glm::vec3 Front = glm::vec3(0.0f, 0.0f, 1.0f);
	alignas(16) glm::vec3 Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
	alignas(16) glm::vec3 Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
};

struct ShaderData
{
	alignas(4) uint32_t uRayCount;

	// minimum allowed throughput...
	alignas(4) float ThroughputFloor = 0.15f;

	// Skybox stuff...
	alignas(4) uint32_t uSkyboxExists = false;
	// The alpha channel contains the rotation of the cube map
	alignas(16) glm::vec4 uSkyboxColor = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f);
};

struct LightProperties
{
	alignas(16) glm::vec3 Color = { 10.0f, 10.0f, 10.0f };
};

struct LightInfo
{
	alignas(4) uint32_t BeginIndex = 0;
	alignas(4) uint32_t Padding = 0;
	alignas(4) uint32_t EndIndex = 0;
	alignas(4) uint32_t LightPropIndex = uint32_t(-1);
};

struct MeshInfo
{
	alignas(4) uint32_t BeginIndex = 0;
	alignas(4) uint32_t Padding = 0;
	alignas(4) uint32_t EndIndex = 0;
	alignas(4) uint32_t MaterialIndex = uint32_t(-1);
};

struct SceneInfo
{
	alignas(8) glm::ivec2 MinBound = glm::ivec2(0, 0);
	alignas(8) glm::ivec2 MaxBound = glm::ivec2(0, 0);
	alignas(8) glm::ivec2 ImageResolution = glm::ivec2(0, 0);

	alignas(4) uint32_t MeshCount = 0;
	alignas(4) uint32_t LightCount = 0;
	alignas(4) uint32_t MinBounceLimit = 1;
	alignas(4) uint32_t MaxBounceLimit = 1;
	alignas(4) uint32_t PixelSamples = 1;

	alignas(4) uint32_t RandomSeed = 0; // Also works as total material count in wavefront path tracer

	alignas(4) uint32_t ResetImage = 1;
	alignas(4) uint32_t FrameCount = 1;
	alignas(4) uint32_t MaxSamples = 4096;
};

struct WavefrontSceneInfo
{
	alignas(8) glm::ivec2 MinBound = glm::ivec2(0, 0);
	alignas(8) glm::ivec2 MaxBound = glm::ivec2(0, 0);
	alignas(8) glm::ivec2 ImageResolution = glm::ivec2(0, 0);

	alignas(4) uint32_t MeshCount = 0;
	alignas(4) uint32_t LightCount = 0;
	alignas(4) uint32_t MaterialCount = 2;
	alignas(4) uint32_t ResetImage = 1;

	alignas(4) uint32_t FrameCount = 1;
};

struct CollisionInfo
{
	// Values set by the collision solver...
	alignas(16) glm::vec3 Normal;
	alignas(4)  float RayDis;
	alignas(16) glm::vec3 IntersectionPoint;
	alignas(4)  float NormalInverted;

	// Intersection primitive info
	alignas(16) glm::vec3 bCoords;
	alignas(4)  uint32_t PrimitiveID;

	// Value set by the collider...
	alignas(4)  uint32_t MaterialIndex;

	// Booleans...
	alignas(4)  bool HitOccured;
	alignas(4)  bool IsLightSrc;
};

struct CameraData
{
	Camera mCamera;

	float mYaw = 0.0f;         // Initialize to 0.0 degrees to look forward
	float mPitch = 0.0f;       // Initialize to 0.0 degrees to look forward
	float mLastX = 400;        // Initial mouse position x
	float mLastY = 300;        // Initial mouse position y
	bool mFirstMouse = true;
};

using VertexBuffer = vkLib::Buffer<glm::vec4>;
using FaceBuffer = vkLib::Buffer<Face>;
using NormalBuffer = vkLib::Buffer<glm::vec4>;
using TexCoordBuffer = vkLib::Buffer<glm::vec2>;

struct Node
{
	alignas(16) glm::vec3 MinBound = glm::vec3();
	alignas(4) uint32_t BeginIndex = 0;

	alignas(16) glm::vec3 MaxBound = glm::vec3();
	alignas(4) uint32_t EndIndex = 0;

	alignas(4) uint32_t FirstChildIndex = 0;
	alignas(4) uint32_t Padding = 0;
	alignas(4) uint32_t SecondChildIndex = 0;
};

using NodeBuffer = vkLib::Buffer<Node>;
using LightPropsBuffer = vkLib::Buffer<LightProperties>;
using CollisionInfoBuffer = vkLib::Buffer<CollisionInfo>;
using RayBuffer = vkLib::Buffer<Ray>;
using RayInfoBuffer = vkLib::Buffer<RayInfo>;

using MeshInfoBuffer = vkLib::Buffer<MeshInfo>;
using LightInfoBuffer = vkLib::Buffer<LightInfo>;

using ShaderDataUniform = vkLib::Buffer<ShaderData>;

struct GeometryBuffers
{
	VertexBuffer Vertices;
	NormalBuffer Normals;
	TexCoordBuffer TexCoords;

	FaceBuffer Faces;

	NodeBuffer Nodes;
};

struct BVH
{
	std::vector<glm::vec3> Vertices;
	std::vector<Face> Faces;
	std::vector<Node> Nodes;
};

struct EstimatorTarget
{
	vkLib::Image PixelMean{};
	vkLib::Image PixelVariance{};
	vkLib::Image Presentable{};

	glm::ivec2 ImageResolution{};
};

struct EstimatorCreateInfo
{
	vkLib::Context Context;
	glm::ivec2 TargetResolution;

	std::string ShaderDirectory;

	glm::ivec2 TileSize;
};

struct TraceInfo
{
	uint32_t MaxSamples = 4096;
	uint32_t BVH_Depth = 8;

	Camera CameraInfo;

	uint32_t MaxBounceLimit = 8;
	uint32_t MinBounceLimit = 3;

	uint32_t PixelSamplesPerBatch = 2;
};

struct WavefrontTraceInfo
{
	uint32_t MaxSamples = 4096;

	glm::mat4 CameraView = glm::mat4(1.0f);
	PhysicalCamera CameraSpecs{};

	uint32_t MaxBounceLimit = 8;
	uint32_t MinBounceLimit = 3;
};

struct Box
{
	glm::vec3 Min;
	glm::vec3 Max;

	Box(const glm::vec3& minBound, const glm::vec3& maxBound)
		: Min(minBound), Max(maxBound) {}

	template <typename Position>
	bool IsInside(const Position& position) const
	{
		return position.x > Min.x && position.y > Min.y && position.z > Min.z &&
			position.x < Max.x && position.y < Max.y && position.z < Max.z;
	}
};

struct Tile
{
	glm::ivec2 Min;
	glm::ivec2 Max;
};

PH_END
AQUA_END
