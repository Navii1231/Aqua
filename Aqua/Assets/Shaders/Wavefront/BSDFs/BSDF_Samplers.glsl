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
