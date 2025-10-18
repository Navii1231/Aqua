#ifndef REFRACTION_BSDF_GLSL
#define REFRACTION_BSDF_GLSL

// Refraction BSDF input structure
struct RefractionBSDF_Input
{
    vec3 ViewDir;               // View direction vector
    vec3 Normal;                // Surface normal vector
    vec3 BaseColor;             // Base color (tint of the glass)
    float RefractiveIndex;      // Refractive index of the glass material
    float Roughness;            // Roughness, for the micro facet model
    float NormalInverted;       // Specifies whether the ray is entering or exiting
};

SampleInfo SampleRefractionBSDF(inout RefractionBSDF_Input bsdfInput)
{
    SampleInfo sampleInfo;

    float RefractiveIndex = bsdfInput.NormalInverted > 0.0 ? 
        1.0 / bsdfInput.RefractiveIndex : bsdfInput.RefractiveIndex;

    sampleInfo.SurfaceNormal = bsdfInput.Normal;
    sampleInfo.iNormal = SampleHalfVecGGXVNDF_Distribution(
        bsdfInput.ViewDir, bsdfInput.Normal, bsdfInput.Roughness);

    sampleInfo.Direction = refract(-bsdfInput.ViewDir, sampleInfo.iNormal, RefractiveIndex);
    sampleInfo.IsInvalid = dot(sampleInfo.Direction, bsdfInput.Normal) >= 0.0;
    sampleInfo.IsReflected = false;

    sampleInfo.Weight = 1.0 / PDF_GGXVNDF_Refraction(-bsdfInput.Normal, bsdfInput.ViewDir, -sampleInfo.iNormal,
        sampleInfo.Direction, bsdfInput.Roughness, bsdfInput.RefractiveIndex);

    // TODO: to calculate
    sampleInfo.Throughput = vec3(1.0);

    return sampleInfo;
}

// Glass BSDF with refraction and transmission using Fresnel-Schlick
vec3 RefractionBSDF(in RefractionBSDF_Input bsdfInput, inout SampleInfo sampleInfo)
{
    float RefractiveIndex = bsdfInput.NormalInverted > 0.0 ?
        1.0 / bsdfInput.RefractiveIndex : bsdfInput.RefractiveIndex;

    vec3 T = bsdfInput.ViewDir;

    if (length(T) == 0.0)
        return vec3(0, 0, 0);

    float DiffSq = RefractiveIndex - 1.0;
    DiffSq *= DiffSq;

    if (DiffSq < EPSILON * EPSILON)
    {
        sampleInfo.Weight = 1.0;
        return vec3(1.0);
    }

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

    vec3 numerator = LdotH * NDF * G2 * R * RefractionJacobian(LdotH, TdotH, RefractiveIndex);
    float denominator = max(abs(NdotT) * NdotL, SHADING_TOLERANCE);

    vec3 radiance = numerator / denominator;

    vec3 Luminance = radiance * bsdfInput.BaseColor * NdotL;

    return Luminance;
}

#endif
