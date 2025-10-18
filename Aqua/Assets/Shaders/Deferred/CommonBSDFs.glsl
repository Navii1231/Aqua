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

    const float MaxGGX_Value = 1.0 / (2.0 * TOLERANCE);

    return min(Value, MaxGGX_Value);
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
