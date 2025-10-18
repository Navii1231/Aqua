#ifndef GLOSSY_BSDF_GLSL
#define GLOSSY_BSDF_GLSL

#define GLOSSY_SAMPLE_SIZE 2

// Specular BSDF input structure
struct GlossyBSDF_Input
{
    vec3 ViewDir;            // View direction vector
    vec3 Normal;             // Surface normal vector
    vec3 BaseColor;          // Base color tint of the specular reflection
    float Roughness;         // Surface roughness for microfacet distribution
};

SampleInfo SampleGlossyBSDF(in GlossyBSDF_Input bsdfInput)
{
	SampleInfo sampleInfo;

	vec3 ViewDir = bsdfInput.ViewDir;
	float Roughness = bsdfInput.Roughness;

	vec3 Normal = bsdfInput.Normal; // TODO: Check out if its a smooth normal

	// Necessary for considering fresnel reflection/refraction probabalities as well as light throughput
	float NdotV = dot(Normal, ViewDir);
	vec3 Reflectivity = bsdfInput.BaseColor;

	// Continue sampling the VNDF Distribution...
	vec3 Directions[GLOSSY_SAMPLE_SIZE];
	vec3 HalfVecs[GLOSSY_SAMPLE_SIZE];
	float SampleWeights[GLOSSY_SAMPLE_SIZE];

	// Visible normal sampling
	HalfVecs[0] = SampleHalfVecGGXVNDF_Distribution(ViewDir, Normal, Roughness);

	// Cosine weighted sampling
	Directions[1] = SampleUnitVecCosineWeighted(Normal);
	Directions[0] = reflect(-ViewDir, HalfVecs[0]);

	HalfVecs[1] = normalize(ViewDir + Directions[1]);

	// Getting the sampling probablity for each aspect of BSDF i.e. BRDF and BTDF
	vec2 SampleProbabilities = GetSampleProbablitiesReflection(Roughness, 1.0, NdotV);

	ivec2 SampleIndexInfo = GetSampleIndex(vec3(SampleProbabilities, 0.0));
	int SampleIndex = SampleIndexInfo[0];

	vec3 LightDir = normalize(Directions[SampleIndex]);
	vec3 H = HalfVecs[SampleIndex];

	// Visible normal sample weights for both BRDF and BTDF
	float VNDF_SampleWeightReflection = PDF_GGXVNDF_Reflection(Normal, ViewDir, H, Roughness);

	float CosineSampleWeight = max(abs(dot(Normal, LightDir)), SHADING_TOLERANCE) / MATH_PI;

	SampleWeights[0] = VNDF_SampleWeightReflection;
	SampleWeights[1] = CosineSampleWeight;

	// MIS BRDF Sampling weight
	float SampleWeightInvReflection = (pow(SampleProbabilities[0] * SampleWeights[0], POWER_HEURISTICS_EXP) +
		pow(SampleProbabilities[1] * SampleWeights[1], POWER_HEURISTICS_EXP)) /
		pow(SampleProbabilities[SampleIndex] * SampleWeights[SampleIndex], POWER_HEURISTICS_EXP - 1.0);

	// Accumulating all the information
	sampleInfo.Direction = LightDir;
	sampleInfo.Weight = 1.0 / max(SampleWeightInvReflection, SHADING_TOLERANCE);
	sampleInfo.iNormal = H;

	sampleInfo.SurfaceNormal = Normal;
	sampleInfo.IsInvalid = dot(LightDir, bsdfInput.Normal) <= 0.0 || SampleIndex > 1;
	sampleInfo.IsReflected = true;

	// Calculating russian roulette weight terms from the fresnel reflection/refraction (turned off for now)

	// ******* TODO: to calculate... **********
	//float FresnelFactor = 1.0;
	//float GGX_Mask = 1.0;

	float NdotH = max(abs(dot(Normal, H)), SHADING_TOLERANCE);

	vec3 FresnelFactor = min(FresnelSchlick(NdotH, vec3(Reflectivity)), 1.0);
	float GGX_Mask = min(GeometrySmith(NdotV, abs(dot(LightDir, Normal)), Roughness), 1.0);

	// prevent it from dropping too much
	sampleInfo.Throughput = clamp(FresnelFactor * GGX_Mask, vec3(0.0), vec3(1.0));
	// ****************************************

	//sampleInfo.Throughput = vec3(1.0);

    return sampleInfo;
}

// Specular BSDF using the Cook-Torrance microfacet model
vec3 GlossyBSDF(in GlossyBSDF_Input bsdfInput, in SampleInfo sampleInfo)
{
    vec3 ViewDir = bsdfInput.ViewDir;

    float NdotH = max(dot(bsdfInput.Normal, sampleInfo.iNormal), SHADING_TOLERANCE);
    float NdotV = max(dot(bsdfInput.Normal, ViewDir), SHADING_TOLERANCE);
    float NdotL = max(dot(bsdfInput.Normal, sampleInfo.Direction), SHADING_TOLERANCE);

    float NDF = DistributionGGX(NdotH, bsdfInput.Roughness);
    float G = GeometrySmith(NdotL, NdotV, bsdfInput.Roughness);
    vec3 F = FresnelSchlick(NdotH, bsdfInput.BaseColor);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL;

    vec3 specular = numerator / denominator;

    vec3 Luminance = specular * NdotL;

    return Luminance;
}

#endif
