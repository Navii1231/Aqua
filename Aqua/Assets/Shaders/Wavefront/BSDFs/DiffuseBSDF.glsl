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
