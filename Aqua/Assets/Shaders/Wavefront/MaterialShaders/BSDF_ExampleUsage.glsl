#version 440
#include "../Wavefront/Common.glsl"

#include "../BSDFs/CommonBSDF.glsl"
#include "../BSDFs/BSDF_Samplers.glsl"

/*
* user defined macro definitions...
* SHADER_TOLERENCE = 0.001, POWER_HEURISTIC_EXP = 2.0,
* EMPTY_MATERIAL_ID = -1, SKYBOX_MATERIAL_ID = -2, LIGHT_MATERIAL_ID = -3,
*/

layout(local_size_x = WORKGROUP_SIZE) in;

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

layout(std430, set = 0, binding = 3) readonly buffer LightInfoBuffer
{
	LightInfo sLightInfos[];
};

layout(std430, set = 0, binding = 4) readonly buffer LightPropsBuffer
{
	LightProperties sLightPropsInfos[];
};

layout(set = 0, binding = 5) uniform sampler2D uCubeMap;

// Post processing for cube map texture
vec3 GammaCorrectionInv(in vec3 color)
{
	return pow(color, vec3(2.2));
}

// Sampling the environment map...
vec3 SampleCubeMap(in vec3 Direction, float Rotation)
{
	// Find the spherical angles of the direction
	float ArcRadius = sqrt(1.0 - Direction.y * Direction.y);
	float Theta = acos(Direction.y);
	float Phi = sign(Direction.z) * acos(Direction.x / ArcRadius);

	// Find the UV coordinates
	float u = (Phi + Rotation + MATH_PI) / (2.0 * MATH_PI);
	float v = Theta / MATH_PI;

	u -= float(int(u));

	vec3 ImageColor = texture(uCubeMap, vec2(u, v)).rgb;

	return GammaCorrectionInv(ImageColor);
}

// Light shader
SampleInfo EvaluateLightShader(in Ray ray, in CollisionInfo collisionInfo)
{
	SampleInfo sampleInfo;

	sampleInfo.Direction = vec3(0.0);
	sampleInfo.iNormal = vec3(0.0);
	sampleInfo.IsInvalid = true;
	sampleInfo.Weight = 1.0;
	sampleInfo.SurfaceNormal = vec3(0.0);
	sampleInfo.Throughput = 0.0;
	
	//sampleInfo.Luminance = sLightPropsInfos[sLightInfos[collisionInfo.MaterialIndex].LightPropsIndex].Color;
	sampleInfo.Luminance = vec3(10.0, 10.0, 10.0);

	return sampleInfo;
}

/************* User defined code (example) *****************/

#include "../BSDFs/DiffuseBSDF.glsl"

SampleInfo Evaluate(in Ray ray, in CollisionInfo collisionInfo)
{
	DiffuseBSDF_Input diffuseInput;
	diffuseInput.ViewDir = -ray.Direction;
	diffuseInput.Normal = collisionInfo.Normal;
	diffuseInput.BaseColor = vec3(0.6, 0.0, 0.6);

	SampleInfo sampleInfo = SampleDiffuseBSDF(diffuseInput);

	sampleInfo.Luminance = DiffuseBSDF(diffuseInput, sampleInfo);

	return sampleInfo;
}

/************************************************************/

SampleInfo EvokeShader(in Ray ray, in CollisionInfo collisionInfo, uint materialRef)
{
	switch (materialRef)
	{
		case LIGHT_MATERIAL_ID:
			// calculate the light shader here...
			return EvaluateLightShader(ray, collisionInfo);
	
		case SKYBOX_MATERIAL_ID:
			// TODO: calculate the skybox shader here...
			SampleInfo sampleInfo;
			sampleInfo.Weight = 1.0;

			if(pSkyboxExists != 0)
				//sampleInfo.Luminance = SampleCubeMap(ray.Direction, pSkyboxColor.w);
				sampleInfo.Luminance = vec3(1.0, 0.0, 0.0);
			else
				sampleInfo.Luminance = pSkyboxColor.rgb;

			return sampleInfo;
	
		default:
			// Calling the placeholder function: SampleInfo Evaluate(in Ray, in CollisionInfo);
			return Evaluate(ray, collisionInfo);
	}
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= pRayCount)
		return;

	Ray ray = sRays[GetActiveIndex(GlobalIdx)];
	CollisionInfo collisionInfo = sCollisionInfos[GetActiveIndex(GlobalIdx)];
	RayInfo rayInfo = sRayInfos[GetActiveIndex(GlobalIdx)];

	uint MaterialRef = ray.MaterialIndex;

	// Dispatch the correct material here, and don't process the inactive rays

	if (ray.Active != 0)
		return;

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
	sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.Luminance * sampleInfo.Weight, 1.0);

	sRays[GetActiveIndex(GlobalIdx)].Origin =
		collisionInfo.IntersectionPoint + collisionInfo.Normal * TOLERENCE;

	sRays[GetActiveIndex(GlobalIdx)].Direction = sampleInfo.Direction;

	if (collisionInfo.HitOccured)
		sRays[GetActiveIndex(GlobalIdx)].Active = collisionInfo.IsLightSrc ? LIGHT_MATERIAL_ID : 0;
	else
		sRays[GetActiveIndex(GlobalIdx)].Active = SKYBOX_MATERIAL_ID;
}
