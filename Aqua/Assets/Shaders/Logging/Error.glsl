#version 440

#ifndef COMMON_GLSL
#define COMMON_GLSL

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


#ifndef COMMON_BSDF_GLSL
#define COMMON_BSDF_GLSL

// This file is included everywhere

#define MATH_PI 3.14159265358979323846

// Helpler functions...
// Fresnel equations approximation
vec3 FresnelSchlick(float HdotV, in vec3 Reflectivity)
{
    return vec3(Reflectivity) + vec3(1.0 - Reflectivity) * pow(1.0 - HdotV, 5.0);
}

float GeometrySchlickGGX(float DotProduct, float Roughness)
{
    float RoughnessSq = Roughness * Roughness;
    float DotProdClamped = max(DotProduct, SHADING_TOLERANCE);

    return (2.0 * DotProdClamped) / (DotProdClamped + sqrt(RoughnessSq +
        (1 - RoughnessSq) * DotProdClamped * DotProdClamped));
}

// Compute geometry term using Smith's method
float GeometrySmith(float NdotV, float NdotL, float Roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, Roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, Roughness);
    return ggx1 * ggx2;
}

// Compute normal distribution function using GGX/Trowbridge-Reitz
float DistributionGGX(float NdotH, float Roughness)
{
    NdotH = max(NdotH, SHADING_TOLERANCE);

    float a2 = Roughness * Roughness;
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = MATH_PI * denom * denom;

    float Value = num / denom;

    const float MaxGGX_Value = 1.0 / (2.0 * SHADING_TOLERANCE);

    return min(Value, MaxGGX_Value);
}

// Only absolute values of IdotH and VdotH are accepted
float RefractionJacobian(float IdotH, float VdotH, float RefractiveIndex)
{
    float Num = RefractiveIndex * RefractiveIndex * abs(VdotH);
    float Denom = IdotH + RefractiveIndex * VdotH;

    float Value = Num / max(Denom * Denom, SHADING_TOLERANCE);

    const float MaxJacobian_Value = 1.0 / (2.0 * SHADING_TOLERANCE);

    return 1.0;

    return clamp(Value, SHADING_TOLERANCE, MaxJacobian_Value);
}

float PDF_GGXVNDF_Reflection(in vec3 Normal, in vec3 ViewDir, in vec3 HalfVec, float Roughness)
{
    float NdotH = max(abs(dot(Normal, HalfVec)), SHADING_TOLERANCE);
    float VdotH = max(abs(dot(ViewDir, HalfVec)), SHADING_TOLERANCE);
    float NdotV = max(abs(dot(Normal, ViewDir)), SHADING_TOLERANCE);

    float G1 = GeometrySchlickGGX(NdotV, Roughness);
    float D_w = DistributionGGX(NdotH, Roughness);

    const float MaxReflectionWeight = 1.0 / (2.0 * SHADING_TOLERANCE);

    return (G1 * D_w) / (4.0 * NdotV);

    return clamp((G1 * D_w) / (4.0 * NdotV), SHADING_TOLERANCE, MaxReflectionWeight);
}

float PDF_GGXVNDF_Refraction(in vec3 Normal, in vec3 ViewDir,
    in vec3 HalfVec, in vec3 IncidentDir, float Roughness, float RefractiveIndex)
{
    float NdotH = max(abs(dot(Normal, HalfVec)), SHADING_TOLERANCE);
    float VdotH = max(abs(dot(ViewDir, HalfVec)), SHADING_TOLERANCE);
    float NdotV = max(abs(dot(ViewDir, Normal)), SHADING_TOLERANCE);
    float IdotH = max(abs(dot(IncidentDir, HalfVec)), SHADING_TOLERANCE);

    float G1 = GeometrySchlickGGX(abs(NdotV), Roughness);
    float D_w = DistributionGGX(NdotH, Roughness);

    const float MaxRefractionWeight = 1.0 / (2.0 * SHADING_TOLERANCE);

    return (G1 * D_w * abs(IdotH)) / abs(NdotV) *
    //return (G1 * D_w * abs(IdotH)) / max(abs(NdotV), SHADING_TOLERANCE) *
        RefractionJacobian(IdotH, VdotH, RefractiveIndex);

    return clamp((G1 * D_w * abs(IdotH)) /
        max(abs(NdotV), SHADING_TOLERANCE) * RefractionJacobian(IdotH, VdotH, RefractiveIndex),
        SHADING_TOLERANCE, MaxRefractionWeight);
}

// Lambertian PDF (cosine-weighted)
float LambertianPDF(in vec3 Normal, in vec3 LightDir)
{
    float NdotL = max(dot(Normal, LightDir), SHADING_TOLERANCE);
    return NdotL / MATH_PI;
}

// Lambertian diffuse BRDF
vec3 LambertianBRDF(in vec3 iNormal, in vec3 LightDir, in vec3 BaseColor)
{
    // Compute the dot product of the surface normal and the light direction
    float NdotL = max(dot(iNormal, LightDir), SHADING_TOLERANCE);

    // Calculate diffuse reflectance using the Lambertian model
    vec3 diffuse = BaseColor / MATH_PI;

    // Multiply by the NdotL term to account for light falloff
    vec3 Lo = diffuse * NdotL;

    return Lo;
}

#endif

#ifndef BSDF_SAMPLERS_GLSL
#define BSDF_SAMPLERS_GLSL

// This file is included everywhere...

uint sRandomSeed;

// For user...
struct SampleInfo
{
    vec3 Direction;
    float Weight;

    vec3 iNormal;
    vec3 SurfaceNormal;

    vec3 Luminance;
    vec3 Throughput;

    bool IsInvalid;
    bool IsReflected;
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

// Function to generate a spherically uniform distribution
vec3 SampleUnitVecUniform(in vec3 Normal)
{
    float u = GetRandom(sRandomSeed);
    float v = GetRandom(sRandomSeed); // Offset to get a different random number
    float phi = u * 2.0 * MATH_PI; // Random azimuthal angle in [0, 2*pi]
    float cosTheta = 2.0 * (v - 0.5); // Random polar angle, acos maps [0,1] to [0,pi]
    float sinTheta = sqrt(1 - cosTheta * cosTheta);

    // Convert spherical coordinates to Cartesian coordinates
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float z = cosTheta;

    vec3 RandomUnit = vec3(x, y, z);

    // Flip the vector if it lies outside of the hemisphere
    return dot(RandomUnit, Normal) < 0.0 ? -RandomUnit : RandomUnit;
}

// Function to generate a cosine weighted distribution
vec3 SampleUnitVecCosineWeighted(in vec3 Normal)
{
    float u = GetRandom(sRandomSeed);
    float v = GetRandom(sRandomSeed); // Offset to get a different random number
    float phi = u * 2.0 * MATH_PI; // Random azimuthal angle in [0, 2*pi]
    float sqrtV = sqrt(v);

    // Convert spherical coordinates to Cartesian coordinates
    float x = sqrt(1.0 - v) * cos(phi);
    float y = sqrt(1.0 - v) * sin(phi);
    float z = sqrtV;

    vec3 Tangent = abs(Normal.x) > abs(Normal.z) ?
        normalize(vec3(Normal.z, 0.0, -Normal.x)) :
        normalize(vec3(0.0, -Normal.z, Normal.y));

    vec3 Bitangent = cross(Normal, Tangent);

    mat3 TBN = mat3(Tangent, Bitangent, Normal);

    return normalize(TBN * vec3(x, y, z));
}

// TODO: This routine needs to be optimised
vec3 SampleHalfVecGGXVNDF_Distribution(in vec3 View, in vec3 Normal, float Roughness)
{
    float u1 = GetRandom(sRandomSeed);
    float u2 = GetRandom(sRandomSeed);

    vec3 Tangent = abs(Normal.x) > abs(Normal.z) ?
        normalize(vec3(Normal.z, 0.0, -Normal.x)) :
        normalize(vec3(0.0, -Normal.z, Normal.y));

    vec3 Bitangent = cross(Normal, Tangent);

    mat3 TBN = mat3(Tangent, Bitangent, Normal);
    mat3 TBNInv = transpose(TBN);

    vec3 ViewLocal = TBNInv * View;

    // Stretch view...
    vec3 StretchedView = normalize(vec3(Roughness * ViewLocal.x,
        Roughness * ViewLocal.y, ViewLocal.z));

    // Orthonormal basis...

    vec3 T1 = StretchedView.z < 0.999 ? normalize(cross(StretchedView, vec3(0.0, 0.0, 1.0))) :
        vec3(1.0, 0.0, 0.0);

    vec3 T2 = cross(T1, StretchedView);

    // Sample point with polar coordinates (r, phi)

    float a = 1.0 / (1.0 + StretchedView.z);
    float r = sqrt(u1);
    float phi = (u2 < a) ? u2 / a * MATH_PI : MATH_PI * (1.0 + (u2 - a) / (1.0 - a));
    float P1 = r * cos(phi);
    float P2 = r * sin(phi) * ((u2 < a) ? 1.0 : StretchedView.z);

    // Form a normal

    vec3 HalfVec = P1 * T1 + P2 * T2 + sqrt(max(0.0, 1.0 - P1 * P1 - P2 * P2)) * StretchedView;

    // Unstretch normal

    HalfVec = normalize(vec3(Roughness * HalfVec.x, Roughness * HalfVec.y, max(0.0, HalfVec.z)));
    HalfVec = TBN * HalfVec;

    return normalize(HalfVec);
}

// Cook Torrance stuff...

#define ROUGHNESS_EXP      2
#define METALLIC_EXP       2
#define MIN_SPECULAR       0.05

float GetDiffuseSpecularSamplingSeparation(float Roughness, float Metallic)
{
    return clamp((pow(Roughness, ROUGHNESS_EXP) + 1.0 - pow(Metallic, METALLIC_EXP)) / 2.0, 0.0, 1.0);
    return 1.0 - (pow(Metallic, METALLIC_EXP) * pow((1.0 - Roughness), ROUGHNESS_EXP) + MIN_SPECULAR) /
        (1.0 + MIN_SPECULAR);
    //return 1.0;
}

vec2 GetTransmissionProbability(float TransmissionWeight, float Metallic,
    float RefractiveIndex, float NdotV, in vec3 RefractionDir)
{
    float Reflectivity = (RefractiveIndex - 1.0) / (RefractiveIndex + 1.0);
    Reflectivity *= Reflectivity;

    vec3 ReflectionFresnel = FresnelSchlick(NdotV, vec3(Reflectivity));
    float ReflectionProb = length(ReflectionFresnel);
    float TransmissionProb = clamp(1.0 - ReflectionProb, 0.0, 1.0) * TransmissionWeight * (1.0 - Metallic);

    //TransmissionProb = TransmissionWeight;
    //TransmissionProb = 0.0;

    TransmissionProb = length(RefractionDir) == 0.0 ? 0.0 : TransmissionProb;

    return vec2(1.0 - TransmissionProb, TransmissionProb);
}

vec3 GetSampleProbablities(float Roughness, float Metallic, float NdotV,
    float TransmissionWeight, float RefractiveIndex, in vec3 RefractionDir)
{
    // [ReflectionDiffuse, ReflectionSpecular, TransmissionDiffuse, TransmissionSpecular]

    float ScatteringSplit = GetDiffuseSpecularSamplingSeparation(Roughness, Metallic);
    vec2 TransmissionSplit = GetTransmissionProbability(TransmissionWeight, Metallic,
        RefractiveIndex, NdotV, RefractionDir);

    vec3 Probs = vec3(0.0, 0.0, 0.0);

    Probs[0] = (1.0 - ScatteringSplit) * TransmissionSplit[0];
    Probs[1] = ScatteringSplit * TransmissionSplit[0];
    Probs[2] = TransmissionSplit[1];

    return Probs;
}

vec2 GetSampleProbablitiesReflection(float Roughness, float Metallic, float NdotV)
{
    // [ReflectionDiffuse, ReflectionSpecular]
    float ScatteringSplit = GetDiffuseSpecularSamplingSeparation(Roughness, Metallic);

    vec2 Probs = vec2(0.0);

    Probs[0] = 1.0 - ScatteringSplit;
    Probs[1] = ScatteringSplit;

    return Probs;
}

ivec2 GetSampleIndex(in vec3 SampleProbablities)
{
    float Xi = GetRandom(sRandomSeed);

    // Accumulate probabilities
    float p0 = SampleProbablities.x;
    float p1 = p0 + SampleProbablities.y;
    float p2 = p1 + SampleProbablities.z;

    // Use step to determine in which region the randomValue falls and cast to int
    int s1 = int(step(p0, Xi)); // 1 if randomValue >= p0, else 0
    int s2 = int(step(p1, Xi)); // 1 if randomValue >= p1, else 0
    int s3 = int(step(p2, Xi)); // 1 if randomValue >= p1, else 0

    // Calculate the index by summing the step results and adding 0.5 offsets
    int SampleIndex = s1 + s2 + s3;

    //SampleIndex = 2;

    int Hemisphere = SampleIndex == 2 ? -1 : 1;

    return ivec2(SampleIndex, Hemisphere);
}

#endif


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


#ifndef UTILS_GLSL
#define UTILS_GLSL

vec2 GetTexCoords(in Face face, in CollisionInfo collisionInfo)
{
	vec2 t1 = sTexCoords[face.Indices.r];
	vec2 t2 = sTexCoords[face.Indices.g];
	vec2 t3 = sTexCoords[face.Indices.b];

	return t1 * collisionInfo.bCoords.r
		+ t2 * collisionInfo.bCoords.g
		+ t3 * collisionInfo.bCoords.b;
}

#endif



struct __PCI_CustomParameterSet
{
	vec3 __pci_base_color;
};


layout(std430, set = SHADER_PARS_SET_IDX, binding = SHADER_PARS_BINDING_IDX) readonly buffer __PCI_ShaderParameterSets
{
	__PCI_CustomParameterSet __sPCI_CustomParameters[];
};
#ifndef DIFFUSE_BSDF_GLSL
#define DIFFUSE_BSDF_GLSL

// Diffuse BSDF input structure
struct DiffuseBSDF_Input
{
    vec3 ViewDir;            // View direction vector
    vec3 Normal;             // Surface Normal
    vec3 BaseColor;          // Base color of the material (albedo)
};

SampleInfo SampleDiffuseBSDF(in DiffuseBSDF_Input bsdfInput)
{
    SampleInfo sampleInfo;

    sampleInfo.Direction = SampleUnitVecCosineWeighted(bsdfInput.Normal);
    sampleInfo.iNormal = normalize(sampleInfo.Direction + bsdfInput.ViewDir);
    sampleInfo.Weight = 1.0 / LambertianPDF(sampleInfo.iNormal, sampleInfo.Direction);
    sampleInfo.SurfaceNormal = bsdfInput.Normal;
    sampleInfo.IsInvalid = false;
    sampleInfo.IsReflected = true;

    // Calculating russian roulette
    float NdotL = dot(bsdfInput.Normal, sampleInfo.Direction);
    float NdotV = dot(bsdfInput.Normal, bsdfInput.ViewDir);

    sampleInfo.Throughput = vec3(NdotL);

    //sampleInfo.Throughput = vec3(1.0);

    return sampleInfo;
}

// Diffuse BSDF function
vec3 DiffuseBSDF(in DiffuseBSDF_Input bsdfInput, in SampleInfo sampleInfo)
{
    return LambertianBRDF(sampleInfo.iNormal, sampleInfo.Direction, bsdfInput.BaseColor);
}

#endif




vec3 Evaluate(BSDFInput bsdfInput)
{
 DiffuseBSDFInput diffuseInput;
 diffuseInput.ViewDir = bsdfInput.ViewDir;
 diffuseInput.LightDir = bsdfInput.LightDir;
 diffuseInput.Normal = bsdfInput.Normal;


 diffuseInput.BaseColor = __sPCI_CustomParameters[0].__pci_base_color;

 return DiffuseBSDF(diffuseInput);
}


// Shader back end of the material pipeline
// Uses the place holder shader: SampleInfo Evaluate(in Ray, in CollisionInfo);

// Post processing for cube map texture
vec3 GammaCorrectionInv(in vec3 color)
{
	return pow(color, vec3(2.2));
}

float NormalizedMagnitude(in vec3 throughput)
{
	// Max value of the magnitude is assumed to be sqrt(3) for vec3(1, 1, 1)
	return length(throughput) / sqrt(3);
}

float MaxComponent(in vec3 vec)
{
	return max(vec.x, max(vec.y, vec.z));
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

// russian roulette
bool ApplyRussianRoulette(inout SampleInfo sampleInfo, inout Ray ray, in vec3 throughput)
{
	float cutoff = NormalizedMagnitude(throughput);

	// prevent the throughput from dropping too much
	cutoff = max(uThroughputFloor, cutoff);

	float Xi = GetRandom(sRandomSeed);
	
	if (Xi > cutoff)
	{
		ray.Active = RR_CUTOFF_CONST;
		return false;
	}
	
	sampleInfo.Luminance /= cutoff;

	return true;
}

// Light shader
SampleInfo EvaluateLightShader(in Ray ray, in CollisionInfo collisionInfo)
{
	SampleInfo sampleInfo;

	sampleInfo.Direction = vec3(0.0);
	sampleInfo.iNormal = vec3(0.0);
	sampleInfo.IsInvalid = false;
	sampleInfo.Weight = 1.0;
	sampleInfo.SurfaceNormal = vec3(0.0);
	sampleInfo.Throughput = vec3(1.0);

	sampleInfo.Luminance = sLightPropsInfos[sLightInfos[collisionInfo.MaterialIndex].LightPropsIndex].Color;

	return sampleInfo;
}

SampleInfo EvokeShader(in Ray ray, in CollisionInfo collisionInfo, in uint materialRef)
{
	// TODO: Can be optimized...
	switch (materialRef)
	{
		case EMPTY_MATERIAL_ID:
			SampleInfo emptySample;
			emptySample.Weight = 1.0;
			emptySample.Luminance = vec3(0.0);
			emptySample.IsInvalid = false;
			emptySample.Throughput = vec3(uThroughputFloor);

			return emptySample;

		case LIGHT_MATERIAL_ID:
			// calculate the light shader here...
			return EvaluateLightShader(ray, collisionInfo);

		case SKYBOX_MATERIAL_ID:
			// TODO: calculate the skybox shader here...
			SampleInfo skyboxInfo;
			skyboxInfo.Weight = 1.0;
			skyboxInfo.IsInvalid = false;
			skyboxInfo.Throughput = vec3(1.0);

			if (uSkyboxExists != 0)
				skyboxInfo.Luminance = SampleCubeMap(ray.Direction, uSkyboxColor.w);
				//skyboxInfo.Luminance = vec3(1.0, 0.0, 0.0);
			else
				skyboxInfo.Luminance = uSkyboxColor.rgb;

			return skyboxInfo;

		default:
			// Calling the placeholder function: SampleInfo Evaluate(in Ray, in CollisionInfo);
			return Evaluate(ray, collisionInfo);
	}
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= uRayCount)
		return;

	if (uRayCount == 0)
		return;

	CollisionInfo collisionInfo = sCollisionInfos[GetActiveIndex(GlobalIdx)];
	RayInfo rayInfo = sRayInfos[GetActiveIndex(GlobalIdx)];
	Ray ray = sRays[GetActiveIndex(GlobalIdx)];

	// make sure that we're dealing with active rays
	if(ray.Active != 0)
		return;

	// Initializing the random numbers
	sRandomSeed = HashCombine(GlobalIdx, pRandomSeed);

	if (sRandomSeed == 0)
		sRandomSeed = 0x9e3770b9;

	// Dispatch the correct material here, and don't process the inactive rays
	uint MaterialRef = ray.MaterialIndex;

	bool MaterialPass = (MaterialRef == pMaterialRef);

	bool InactivePass = (MaterialRef == LIGHT_MATERIAL_ID ||
		MaterialRef == EMPTY_MATERIAL_ID ||
		MaterialRef == SKYBOX_MATERIAL_ID) &&
		(pMaterialRef == -1);

	if (!MaterialPass && !InactivePass)
		return;

	// Sampling and dispatching to the approapriate shader
	SampleInfo sampleInfo;
	sampleInfo = EvokeShader(ray, collisionInfo, MaterialRef);

	// enforce the russian roulette constraints
	if (pBounceCount < 4)
		sampleInfo.Throughput = vec3(1.0);

	// insert the luminance throughput
	rayInfo.Throughput.xyz *= sampleInfo.Throughput;
	sRayInfos[GetActiveIndex(GlobalIdx)].Throughput = rayInfo.Throughput;

	// apply russian roulette
	ApplyRussianRoulette(sampleInfo, sRays[GetActiveIndex(GlobalIdx)], rayInfo.Throughput.xyz);

	// Update the color values and ray directions
	if(!sampleInfo.IsInvalid)
		sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.Luminance * sampleInfo.Weight, 1.0);
		//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.Luminance, 1.0);
		//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.iNormal * sampleInfo.Weight, 1.0);
		//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.iNormal, 1.0);
	else
		sRayInfos[GetActiveIndex(GlobalIdx)].Luminance = vec4(0.0, 0.0, 0.0, 1.0);

	float sign = sampleInfo.IsReflected ? 1.0 : -1.0;

	// Aim at the sampled direction
	sRays[GetActiveIndex(GlobalIdx)].Origin =
		collisionInfo.IntersectionPoint + sign * collisionInfo.Normal * SHADING_TOLERANCE;

	sRays[GetActiveIndex(GlobalIdx)].Direction = sampleInfo.Direction;

	// Figure out if the ray went into a terminating material
	if (collisionInfo.HitOccured)
		sRays[GetActiveIndex(GlobalIdx)].Active = collisionInfo.IsLightSrc ? LIGHT_MATERIAL_ID : 0;
	else
		sRays[GetActiveIndex(GlobalIdx)].Active = SKYBOX_MATERIAL_ID;

	if (sampleInfo.IsInvalid)
		sRays[GetActiveIndex(GlobalIdx)].Active = EMPTY_MATERIAL_ID;
}


