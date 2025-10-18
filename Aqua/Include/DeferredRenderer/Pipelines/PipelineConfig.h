#pragma once
#include "../../Core/AqCore.h"
#include "../Renderable/VertexFactory.h"
#include "../../Core/SharedRef.h"

AQUA_BEGIN

// The will specify the resource layout
// Entries: "@position", "@normal", "@tangent_space", "@texture_coords", 
// "@material_ids", "@index"

struct VaryingAttribute
{
	std::string Name;
	std::string Format;

	void SetName(const std::string& name) { Name = name; }
	void SetImageFormat(const std::string& name) { Format = name; }
};

struct CameraInfo
{
	glm::mat4 Projection;
	glm::mat4 View;
};

struct Resource
{
	vkLib::DescriptorLocation Location;

	vkLib::Core::BufferResource Buffer;
	vkLib::ImageView ImageView;
};

/* An example of a shading pipeline */
struct PBRMaterial
{
	alignas(16) glm::vec4 BaseColor;
	alignas(4) float Roughness;
	alignas(4) float Metallic;
	alignas(4) float RefractiveIndex;
	alignas(4) float TransmissionWeight;
};

struct DirectionalLightSrc
{
	alignas(16) glm::vec4 Color;
	alignas(16) glm::vec4 Direction;
};

struct PointLightSrc
{
	alignas(16) glm::vec4 Position;
	alignas(16) glm::vec4 Intensity;
	alignas(16) glm::vec4 Color;
	alignas(8) glm::vec2 DropRate;
};

using FragmentAttributes = std::vector<VaryingAttribute>;

using FragmentResourceMap = std::unordered_map<std::string, vkLib::Image>;
using ResourceMap = std::unordered_map<std::string, Resource>;
using Mat4Buf = vkLib::Buffer<glm::mat4>;
using CameraBuf = vkLib::Buffer<CameraInfo>;
using DepthCameraBuf = vkLib::Buffer<CameraInfo>;

AQUA_END
