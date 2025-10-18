#ifndef RANDOM_GLSL
#define RANDOM_GLSL

#define MATH_PI 3.14159265358979323846

float GetRandom(inout uint state)
{
	state *= state * 747796405 + 2891336453;
	uint result = ((state >> (state >> 28) + 4) ^ state) * 277803737;
	result = (result >> 22) ^ result;
	return result / 4294967295.0;
}

// Function to generate a spherically uniform distribution
vec3 SampleUnitVecUniform(in vec3 Normal, inout uint state) 
{
    float u = GetRandom(state);
    float v = GetRandom(state); // Offset to get a different random number
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
vec3 SampleUnitVecCosineWeighted(in vec3 Normal, inout uint state) 
{
    float u = GetRandom(state);
    float v = GetRandom(state); // Offset to get a different random number
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
vec3 SampleHalfVecGGXVNDF_Distribution(in vec3 View, in vec3 Normal, float Roughness, inout uint state)
{
	float u1 = GetRandom(state);
	float u2 = GetRandom(state);

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

#endif