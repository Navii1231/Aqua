#ifndef GLASS_BSDF_GLSL
#define GLASS_BSDF_GLSL

#define GLASS_SAMPLE_SIZE  3

// Includes the CookTorrance shader

// Glass BSDF input structure
struct GlassBSDF_Input
{
    vec3 ViewDir;
    float Roughness;

    vec3 Normal;
    float RefractiveIndex;

    vec3 BaseColor;
	float NormalInverted;
};

SampleInfo SampleGlassBSDF(in GlassBSDF_Input bsdfInput)
{
	SampleInfo sampleInfo;

	vec3 ViewDir = bsdfInput.ViewDir;
	float Roughness = bsdfInput.Roughness;
	float RefractiveIndex = bsdfInput.NormalInverted > 0.0 ?
		1.0 / bsdfInput.RefractiveIndex : bsdfInput.RefractiveIndex;

	vec3 Normal = bsdfInput.Normal; // TODO: Check if its a smooth normal

	/* TODO: Calculation for the smooth (turned off for now)
	vec3 FarTangent = normalize(IncomingDirection -
		dot(IncomingDirection, HitInfo.Normal) * Normal + TOLERANCE * Normal);

	vec3 FarHalfVec = normalize(ViewDir + FarTangent); // This is the furthest away half vector you can have

	// Interpolate the smooth normal with geometric normal based on the angle
	Normal = mix(Normal, HitInfo.Normal, pow(1.0 - dot(HitInfo.Normal, ViewDir), 5));

	// If the normal is pointing further away from the far half vector, then clamp the normal to the far half vector
	// This effectively eliminates any artifacts that may occur due to smooth shading
	if (dot(Normal, FarTangent) > dot(FarHalfVec, FarTangent))
		Normal = FarHalfVec;
	*/

	// Necessary for considering fresnel reflection/refraction probabalities as well as light throughput
	float NdotV = dot(Normal, ViewDir);
	float Reflectivity = (RefractiveIndex - 1.0) / (RefractiveIndex + 1.0);
	Reflectivity = min(Reflectivity * Reflectivity, 1.0);

	// Continue sampling the Cook Torrance BSDF...
	vec3 Directions[GLASS_SAMPLE_SIZE];
	vec3 HalfVecs[GLASS_SAMPLE_SIZE];
	float SampleWeights[GLASS_SAMPLE_SIZE];

	// Visible normal sampling
	HalfVecs[0] = SampleHalfVecGGXVNDF_Distribution(ViewDir, Normal, Roughness);

	// Cosine weighted sampling
	Directions[1] = SampleUnitVecCosineWeighted(Normal);

	HalfVecs[2] = HalfVecs[0];
	HalfVecs[1] = normalize(ViewDir + Directions[1]);

	Directions[0] = reflect(-ViewDir, HalfVecs[0]);
	Directions[2] = refract(-ViewDir, HalfVecs[0], RefractiveIndex);

	// Getting the sampling probablity for each aspect of BSDF i.e. BRDF and BTDF
	// Difference between full CookTorrance is that 
	// the metallic factor is 0, and the transmission weight is 1
	vec3 SampleProbabilities = GetSampleProbablities(Roughness, 0.0, NdotV,
		1.0, RefractiveIndex, Directions[2]);

	ivec2 SampleIndexInfo = GetSampleIndex(SampleProbabilities);
	int SampleIndex = SampleIndexInfo[0];
	float HemisphereSide = float(SampleIndexInfo[1]);

	vec3 LightDir = normalize(Directions[SampleIndex]);
	vec3 H = HalfVecs[SampleIndex];

	// Visible normal sample weights for both BRDF and BTDF
	float VNDF_SampleWeightReflection = PDF_GGXVNDF_Reflection(Normal, ViewDir, H, Roughness);
	float VNDF_SampleWeightRefraction = PDF_GGXVNDF_Refraction(-Normal, ViewDir, -H,
		LightDir, Roughness, RefractiveIndex);

	float CosineSampleWeight = max(abs(dot(Normal, LightDir)), SHADING_TOLERANCE) / MATH_PI;

	SampleWeights[0] = VNDF_SampleWeightReflection;
	SampleWeights[1] = CosineSampleWeight;
	SampleWeights[2] = VNDF_SampleWeightRefraction;

	// MIS BRDF Sampling weight
	float SampleWeightInvReflection = (pow(SampleProbabilities[0] * SampleWeights[0], POWER_HEURISTICS_EXP) +
		pow(SampleProbabilities[1] * SampleWeights[1], POWER_HEURISTICS_EXP)) /
		pow(SampleProbabilities[SampleIndex] * SampleWeights[SampleIndex], POWER_HEURISTICS_EXP - 1.0);

	// MIS BTDF Sampling weight
	float SampleWeightInvRefraction = SampleProbabilities[2] * SampleWeights[2];

	// Combining the BRDF and BTDF sampling weights
	float SampleWeightInv = HemisphereSide > 0.0 ? SampleWeightInvReflection : SampleWeightInvRefraction;

	// Accumulating all the information
	sampleInfo.Direction = LightDir;
	sampleInfo.Weight = 1.0 / max(SampleWeightInv, SHADING_TOLERANCE);
	sampleInfo.iNormal = H;

	sampleInfo.SurfaceNormal = Normal;
	sampleInfo.IsInvalid = dot(LightDir, bsdfInput.Normal) * HemisphereSide <= 0.0 || SampleIndex > 2;
	sampleInfo.IsReflected = HemisphereSide > 0.0;

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

	sampleInfo.Throughput = vec3(1.0);

	return sampleInfo;
}

vec3 GlassBRDF(in GlassBSDF_Input bsdfInput, in SampleInfo sampleInfo)
{
#if _DEBUG
	if (dot(V, N) < 0.0)
		return vec3(1.0, 0.0, 0.0);

	if (dot(L, N) < 0.0)
		return vec3(1.0, 0.0, 0.0);
#endif

	vec3 ViewDir = bsdfInput.ViewDir;

	float NdotH = max(dot(bsdfInput.Normal, sampleInfo.iNormal), SHADING_TOLERANCE);
	float NdotV = max(dot(bsdfInput.Normal, ViewDir), SHADING_TOLERANCE);
	float NdotL = max(dot(bsdfInput.Normal, sampleInfo.Direction), SHADING_TOLERANCE);

	vec3 Reflectivity = vec3((bsdfInput.RefractiveIndex - 1.0) /
		(bsdfInput.RefractiveIndex + 1.0));

	Reflectivity *= Reflectivity;

	float NDF = DistributionGGX(NdotH, bsdfInput.Roughness);
	float G = GeometrySmith(NdotL, NdotV, bsdfInput.Roughness);
	vec3 F = FresnelSchlick(NdotH, Reflectivity);

	vec3 numerator = NDF * G * F;
	float denominator = 4.0 * NdotV * NdotL;

	vec3 specular = numerator / denominator;
	vec3 kS = F;

	vec3 Lo = specular * NdotL;

	return Lo;
}

// Glass BTDF with refraction and transmission using Fresnel-Schlick
vec3 GlassBTDF(in GlassBSDF_Input bsdfInput, in SampleInfo sampleInfo)
{
	float RefractiveIndex = bsdfInput.NormalInverted > 0.0 ?
		1.0 / bsdfInput.RefractiveIndex : bsdfInput.RefractiveIndex;

    vec3 T = bsdfInput.ViewDir;

    if (length(T) == 0.0)
        return vec3(0, 0, 0);

    if (RefractiveIndex == 1.0)
        return vec3(1.0);

    T = normalize(T);

    float NdotH = max(dot(bsdfInput.Normal, sampleInfo.iNormal), SHADING_TOLERANCE);
    float NdotL = max(dot(-bsdfInput.Normal, sampleInfo.Direction), SHADING_TOLERANCE);
    float NdotT = dot(-bsdfInput.Normal, T);

    float LdotH = max(dot(sampleInfo.Direction, -sampleInfo.iNormal), SHADING_TOLERANCE);
    float TdotH = dot(T, -sampleInfo.iNormal);

    vec3 Reflectivity = vec3((RefractiveIndex - 1.0) / (RefractiveIndex + 1.0));
    Reflectivity *= Reflectivity;

    float NDF = DistributionGGX(NdotH, bsdfInput.Roughness);
    float G2 = GeometrySmith(abs(NdotT), NdotL, bsdfInput.Roughness);

    vec3 F = FresnelSchlick(NdotH, Reflectivity);

    vec3 R = vec3(1.0) - F;

	vec3 numerator = LdotH * NDF * G2 * R * RefractionJacobian(abs(LdotH), abs(TdotH), RefractiveIndex);
    float denominator = max(abs(NdotT) * NdotL, SHADING_TOLERANCE);

    vec3 radiance = numerator / denominator;

    vec3 Luminance = radiance * bsdfInput.BaseColor * NdotL;

    return Luminance;
}

vec3 GlassBSDF(in GlassBSDF_Input bsdfInput, in SampleInfo sampleInfo)
{
	float alpha = sampleInfo.IsReflected ? 0.0 : 1.0;

	return mix(GlassBRDF(bsdfInput, sampleInfo), GlassBTDF(bsdfInput, sampleInfo), alpha);
}

#endif
