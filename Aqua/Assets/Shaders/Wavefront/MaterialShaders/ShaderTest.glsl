/* Shader front end of the material pipeline
*
* Contains stuff like:
* #include "../Wavefront/Common.glsl"
* #include "../BSDFs/CommonBSDF.glsl"
* #include "../BSDFs/BSDF_Samplers.glsl"
*
* user defined macro definitions...
* SHADER_TOLERENCE = 0.001, POWER_HEURISTIC_EXP = 2.0,
* EMPTY_MATERIAL_ID = -1, SKYBOX_MATERIAL_ID = -2, LIGHT_MATERIAL_ID = -3,
*/

#version 440 core

layout(local_size_x = WORKGROUP_SIZE) in;

#ifndef COMMON_GLSL
#define COMMON_GLSL

uint sRandomSeed;

// For user...
struct SampleInfo
{
	vec3 Direction;
	float Throughput;

	vec3 iNormal;
	float Weight;

	vec3 SurfaceNormal;

	bool IsInvalid;
	bool IsReflected;

	vec3 Luminance;
};

uint Hash(uint state)
{
	state *= state * 747796405 + 2891336453;
	uint result = ((state >> (state >> 28) + 4) ^ state) * 277803737;
	return (result >> 22) ^ result;
}

float GetRandom(inout uint state)
{
	state *= state * 747796405 + 2891336453;
	uint result = ((state >> (state >> 28) + 4) ^ state) * 277803737;
	result = (result >> 22) ^ result;
	return result / 4294967295.0;
}

uint HashCombine(uint seed1, uint seed2)
{
	uint combined = seed1;
	combined ^= seed2 + 0x9e3779b9 + (combined << 6) + (combined >> 2);

	if (combined == 0)
		combined = 0x9e3770b9;

	return Hash(combined);
}

struct Ray
{
	vec3 Origin;
	uint MaterialIndex;
	vec3 Direction;
	uint Active; // Zero means active; any other means inactive
};

struct RayInfo
{
	uvec2 ImageCoordinate;
	vec4 Luminance;
	vec4 Throughput;
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

struct CollisionInfo
{
	// Values set by the collision solver...
	vec3 Normal;
	float RayDis;
	vec3 IntersectionPoint;
	float NormalInverted;

	// Intersection primitive info
	vec3 bCoords;
	uint PrimitiveID;

	// Value set by the collider...
	uint MaterialIndex;

	// Booleans...
	bool HitOccured;
	bool IsLightSrc;
};

struct RayRef
{
	uint MaterialIndex;
	uint FieldIndex;
};

vec3 GetPoint(Ray ray, float Par)
{
	return ray.Origin + ray.Direction * Par;
}

#endif


layout(push_constant) uniform ShaderData
{
	// Material and ray stuff...
	uint pMaterialRef;
	uint pRayCount;
	uint pActiveBuffer;
	uint pRandomSeed;

	// Skybox stuff...
	vec4 pSkyboxColor; // The alpha channel contains the rotation of the cube map
	uint pSkyboxExists;
};

struct Face
{
	uvec4 Indices;

	uint MaterialRef;
	uint Padding1;

	uint FaceID;
	uint Padding2;
};

uint GetActiveIndex(uint index)
{
	return pRayCount * pActiveBuffer + index;
}

uint GetInactiveIndex(uint index)
{
	return pRayCount * (1 - pActiveBuffer) + index;
}

layout(std430, set = 0, binding = 0) buffer RayBuffer
{
	Ray sRays[];
};

layout(std430, set = 0, binding = 1) buffer RayInfoBuffer
{
	RayInfo sRayInfos[];
};

layout(std430, set = 0, binding = 2) buffer CollisionInfoBuffer
{
	CollisionInfo sCollisionInfos[];
};

layout(std430, set = 0, binding = 3) readonly buffer VertexBuffer
{
	vec3 sPositions[];
};

layout(std430, set = 0, binding = 4) readonly buffer NormalBuffer
{
	vec3 sNormals[];
};

layout(std430, set = 0, binding = 5) readonly buffer TexCoordBuffer
{
	vec2 sTexCoords[];
};

layout(std430, set = 0, binding = 6) readonly buffer FaceBuffer
{
	Face sFaces[];
};

layout(std430, set = 0, binding = 7) readonly buffer LightInfoBuffer
{
	LightInfo sLightInfos[];
};

layout(std430, set = 0, binding = 8) readonly buffer LightPropsBuffer
{
	LightProperties sLightPropsInfos[];
};

layout(set = 0, binding = 9) uniform sampler2D uCubeMap;

SampleInfo EvokeShader(Ray ray, CollisionInfo collisionInfo, uint MaterialRef)
{
	SampleInfo result;
	return result;
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= pRayCount)
		return;

	// Bottleneck: multiple pipelines are reading the same memory
	// There is one pipeline that doesn't even need this data

	CollisionInfo collisionInfo = sCollisionInfos[GetActiveIndex(GlobalIdx)];
	RayInfo rayInfo = sRayInfos[GetActiveIndex(GlobalIdx)];
	Ray ray = sRays[GetActiveIndex(GlobalIdx)];

	if (pRayCount != 0)
		return;

	uint MaterialRef = ray.MaterialIndex;

	// Dispatch the correct material here, and don't process the inactive rays

	bool MaterialPass = (MaterialRef == pMaterialRef);

	bool InactivePass = (MaterialRef == LIGHT_MATERIAL_ID ||
		MaterialRef == EMPTY_MATERIAL_ID ||
		MaterialRef == SKYBOX_MATERIAL_ID) &&
		(pMaterialRef == -1);

	if (!(MaterialPass || InactivePass))
		return;

	sRandomSeed = HashCombine(GlobalIdx, pRandomSeed);

	if (sRandomSeed == 0)
		sRandomSeed = 0x9e3770b9;

	SampleInfo sampleInfo = EvokeShader(ray, collisionInfo, MaterialRef);

	// Update the color values and ray directions
	if (!sampleInfo.IsInvalid)
		sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.Luminance * sampleInfo.Weight, 1.0);
	//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.Luminance, 1.0);
	//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.iNormal * sampleInfo.Weight, 1.0);
	//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.iNormal, 1.0);
	else
		sRayInfos[GetActiveIndex(GlobalIdx)].Luminance = vec4(0.0, 0.0, 0.0, 1.0);

	float sign = sampleInfo.IsReflected ? 1.0 : -1.0;

	sRays[GetActiveIndex(GlobalIdx)].Origin =
		collisionInfo.IntersectionPoint + sign * collisionInfo.Normal * TOLERENCE;

	sRays[GetActiveIndex(GlobalIdx)].Direction = sampleInfo.Direction;

	if (collisionInfo.HitOccured)
		sRays[GetActiveIndex(GlobalIdx)].Active = collisionInfo.IsLightSrc ? LIGHT_MATERIAL_ID : 0;
	else
		sRays[GetActiveIndex(GlobalIdx)].Active = SKYBOX_MATERIAL_ID;

	if (sampleInfo.IsInvalid)
		sRays[GetActiveIndex(GlobalIdx)].Active = EMPTY_MATERIAL_ID;

}