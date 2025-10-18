#ifndef DESC_SET_1_GLSL
#define DESC_SET_1_GLSL

// Note: Set 1 is reserved for the estimator!

struct Face
{
	uvec4 Indices;

	uint MaterialRef;
	uint Padding1;

	uint FaceID;
	uint Padding2;
};

layout(std430, set = 1, binding = 0) readonly buffer VertexBuffer
{
	vec3 sPositions[];
};

layout(std430, set = 1, binding = 1) readonly buffer NormalBuffer
{
	vec3 sNormals[];
};

layout(std430, set = 1, binding = 2) readonly buffer TexCoordBuffer
{
	vec2 sTexCoords[];
};

layout(std430, set = 1, binding = 3) readonly buffer FaceBuffer
{
	Face sFaces[];
};

layout(std430, set = 1, binding = 4) readonly buffer NodeBuffer
{
	Node sNodes[];
};

// Temporary... will be removed once the runtime material evaluation is implemented
layout(std430, set = 1, binding = 5) readonly buffer MaterialBuffer
{
	Material sMaterialInfos[];
};

layout(std430, set = 1, binding = 6) readonly buffer LightPropsBuffer
{
	LightProperties sLightPropsInfos[];
};

layout(std430, set = 1, binding = 7) readonly buffer MeshInfoBuffer
{
	MeshInfo sMeshInfos[];
};

layout(std430, set = 1, binding = 8) readonly buffer LightInfoBuffer
{
	LightInfo sLightInfos[];
};

layout(std140, set = 1, binding = 9) uniform SceneInfo
{
	ivec2 MinBound;
	ivec2 MaxBound;
	ivec2 ImageResolution;

	uint MeshCount;
	uint LightCount;
	uint MaterialCount;

	uint ResetImage;
	uint FrameCount;
} uSceneInfo;

layout(set = 1, binding = 10) uniform sampler2D uCubeMap;

#endif