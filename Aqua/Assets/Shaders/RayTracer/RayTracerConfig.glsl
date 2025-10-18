#ifndef RAY_TRACER_CONFIG_GLSL
#define RAY_TRACER_CONFIG_GLSL

// Note: Set 1 is reserved for the estimator!

struct Ray
{
	vec3 Origin;
	vec3 Direction;
};

struct Material
{
	vec3 Albedo;
	float Metallic;
	float Roughness;
	float Transmission;
	float Padding1;
	float RefractiveIndex;
};

struct LightProperties
{
	vec3 Color;
};

struct MeshInfo
{
	uint BeginIndex;
	uint Padding;
	uint EndIndex;
	uint MaterialIndex;
};

struct LightInfo
{
	uint BeginIndex;
	uint Padding;
	uint EndIndex;
	uint LightPropsIndex;
};

struct Node
{
	vec3 MinBound;
	uint BeginIndex;

	vec3 MaxBound;
	uint EndIndex;

	uint FirstChildIndex;
	uint Padding;
	uint SecondChildIndex;
};

vec3 GetPoint(Ray ray, float Par)
{
	return ray.Origin + ray.Direction * Par;
}

layout (std140, set = 1, binding = 0) uniform CameraUniform
{
	vec3 Position;
	vec3 Direction;
	vec3 Tangent;
	vec3 Bitangent;
} uCamera;

layout (std430, set = 1, binding = 1) readonly buffer VertexBuffer
{
	vec3 Positions[];
} sVertexBuffer;

layout (std430, set = 1, binding = 2) readonly buffer NormalBuffer
{
	vec3 Normals[];
} sNormalBuffer;

layout (std430, set = 1, binding = 3) readonly buffer TexCoordBuffer
{
	vec2 TexCoords[];
} sTexCoordsBuffer;

layout (std430, set = 1, binding = 4) readonly buffer IndexBuffer
{
	uvec4 Indices[];
} sIndexBuffer;

layout (std430, set = 1, binding = 5) readonly buffer NodeBuffer
{
	Node Nodes[];
} sNodeBuffer;

// Temporary... will be removed once the runtime material evaluation is implemented
layout(std430, set = 1, binding = 6) readonly buffer MaterialBuffer
{
	Material Infos[];
} sMaterialBuffer;

layout(std430, set = 1, binding = 7) readonly buffer LightPropsBuffer
{
	LightProperties Infos[];
} sLightPropsBuffer;

layout (std430, set = 1, binding = 8) readonly buffer MeshInfoBuffer
{
	MeshInfo Infos[];
} sMeshInfos;

layout (std430, set = 1, binding = 9) readonly buffer LightInfoBuffer
{
	LightInfo Infos[];
} sLightInfos;

layout (std140, set = 1, binding = 10) uniform SceneInfo
{
	ivec2 MinBound;
	ivec2 MaxBound;
	ivec2 ImageResolution;

	uint MeshCount;
	uint LightCount;
	uint MinBounceLimit;
	uint MaxBounceLimit;
	uint PixelSamples;

	uint RandomSeed;
	uint ResetImage;
	uint FrameCount;
	uint MaxSamples;
} uSceneInfo;

layout (set = 1, binding = 11) uniform sampler2D uCubeMap;

#endif
