/* Shader front end of the material pipeline
* 
* Contains stuff like:
* #include "../Wavefront/Common.glsl"
* #include "../BSDFs/CommonBSDF.glsl"
* #include "../BSDFs/BSDF_Samplers.glsl"
* 
* user defined macro definitions...
* SHADER_TOLERANCE = 0.001, POWER_HEURISTIC_EXP = 2.0,
* EMPTY_MATERIAL_ID = -1, SKYBOX_MATERIAL_ID = -2, LIGHT_MATERIAL_ID = -3,
* RR_CUTOFF_CONST = -4 (indicates that the path was terminated through russian roulette)
*/

layout(local_size_x = WORKGROUP_SIZE) in;

layout(push_constant) uniform ShaderConstants
{
	// Material and ray stuff...
	uint pMaterialRef;
	uint pActiveBuffer;
	uint pRandomSeed;
	uint pBounceCount;
};

struct Face
{
	uvec4 Indices;

	uint MaterialRef;
	uint Padding1;

	uint FaceID;
	uint Padding2;
};

// TODO: configure the set binding indices through macros

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

layout(std140, set = 1, binding = 0) uniform ShaderData
{
	uint uRayCount;
	float uThroughputFloor; // minimum russian roulette probability

	// Skybox stuff...
	uint uSkyboxExists;
	vec4 uSkyboxColor; // The alpha channel holds the rotation of the cube map
};

uint GetActiveIndex(uint index)
{
	//return index;
	return uRayCount * pActiveBuffer + index;
}

uint GetInactiveIndex(uint index)
{
	//return uRayCount + index;
	return uRayCount * (1 - pActiveBuffer) + index;
}
