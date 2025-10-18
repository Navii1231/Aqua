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