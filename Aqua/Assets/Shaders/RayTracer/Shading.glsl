#ifndef SHADING_GLSL
#define SHADING_GLSL

#define MATH_PI 3.14159265358979323846
#define SHADING_TOLERANCE 0.0001

// Physically based BSDF structures...

struct BSDF_Input
{
    vec3 LightDir;
    float InterfaceRoughness;

    vec3 Normal;
    float InterfaceMetallic;

    vec3 HalfVec;
    float TransmissionWeight;

    vec3 BaseColor;
    float RefractiveIndexRatio;
};

// Post-processing...

vec3 ToneMap(in vec3 color, float exposure)
{
    return vec3(1.0) - exp(-color * exposure);
}

vec3 GammaCorrection(in vec3 color)
{
    return pow(color, vec3(1.0 / 2.2));
}

vec3 GammaCorrectionInv(in vec3 color)
{
    return pow(color, vec3(2.2));
}

float RefractionJacobian(float IdotH, float VdotH, float RefractiveIndex)
{
    float Num = RefractiveIndex * RefractiveIndex * abs(VdotH);
    float Denom = IdotH + RefractiveIndex * VdotH;

    float Value = Num / max(Denom * Denom, SHADING_TOLERANCE);

    const float MaxJacobian_Value = 1.0 / (2.0 * TOLERANCE);

    return clamp(Value, SHADING_TOLERANCE, MaxJacobian_Value);
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

// Blinn Phong model...

vec3 BlinnPhongBRDF(in vec3 L, in vec3 V, in vec3 N, in Material SurfaceProps)
{
    vec3 H = normalize(L + V);

    float NdotH = dot(N, H);
    float NdotL = dot(N, L);

    float Shininess = 512.0 * (1.0 - SurfaceProps.Roughness);

    float RefractiveIndex = SurfaceProps.RefractiveIndex;

    vec3 Reflectivity = vec3((RefractiveIndex - 1.0) / (RefractiveIndex - 1.0));
    Reflectivity *= Reflectivity;

    vec3 DiffuseAmplitude = vec3(1.0) - Reflectivity;
    DiffuseAmplitude = max(vec3(0.0), DiffuseAmplitude);

    float Diffuse = max(NdotL, 0.0);
    float Specular = pow(max(NdotH, 0.0), Shininess);

    return Diffuse * SurfaceProps.Albedo * DiffuseAmplitude + Specular * Reflectivity;
}

// PBR model...

// Fresnel equations approximation
vec3 FresnelSchlick(float HdotV, in vec3 Reflectivity)
{
	return Reflectivity + (vec3(1.0) - Reflectivity) * pow(1.0 - HdotV, 5.0);
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

    return clamp(Value, SHADING_TOLERANCE, MaxGGX_Value);
}

float PDF_GGXVNDF_Reflection(in vec3 Normal, in vec3 ViewDir, in vec3 HalfVec, float Roughness)
{
    float NdotH = max(dot(Normal, HalfVec), SHADING_TOLERANCE);
    float VdotH = max(dot(ViewDir, HalfVec), SHADING_TOLERANCE);
    float NdotV = max(dot(Normal, ViewDir), SHADING_TOLERANCE);

    float G1 = GeometrySchlickGGX(NdotV, Roughness);
    float D_w = DistributionGGX(NdotH, Roughness);
    
    return (G1 * D_w) / (4.0 * NdotV);
}

float PDF_GGXVNDF_Refraction(in vec3 Normal, in vec3 ViewDir,
    in vec3 HalfVec, in vec3 IncidentDir, float Roughness, float RefractiveIndex)
{
    float NdotH = dot(Normal, HalfVec);
    float VdotH = dot(ViewDir, HalfVec);
    float NdotV = dot(ViewDir, Normal);
    float IdotH = dot(IncidentDir, HalfVec);

    float G1 = GeometrySchlickGGX(abs(NdotV), Roughness);
    float D_w = DistributionGGX(NdotH, Roughness);

    return (G1 * D_w * abs(IdotH)) /
        max(abs(NdotV), SHADING_TOLERANCE) * RefractionJacobian(IdotH, VdotH, RefractiveIndex);
}

vec3 CookTorranceBRDF(in BSDF_Input bsdfInput)
{
#if _DEBUG
    if(dot(V, N) < 0.0)
        return vec3(1.0, 0.0, 0.0);

    if(dot(L, N) < 0.0)
        return vec3(1.0, 0.0, 0.0);
#endif
    vec3 ViewDir = reflect(-bsdfInput.LightDir, bsdfInput.HalfVec);

    float NdotH = max(dot(bsdfInput.Normal, bsdfInput.HalfVec), SHADING_TOLERANCE);
    float NdotV = max(dot(bsdfInput.Normal, ViewDir), SHADING_TOLERANCE);
    float NdotL = max(dot(bsdfInput.Normal, bsdfInput.LightDir), SHADING_TOLERANCE);

    vec3 Reflectivity = vec3((bsdfInput.RefractiveIndexRatio - 1.0) /
        (bsdfInput.RefractiveIndexRatio + 1.0));

    Reflectivity *= Reflectivity;

    Reflectivity = mix(Reflectivity, bsdfInput.BaseColor, bsdfInput.InterfaceMetallic);

    float NDF = DistributionGGX(NdotH, bsdfInput.InterfaceRoughness);
    float G = GeometrySmith(NdotL, NdotV, bsdfInput.InterfaceRoughness);
    vec3 F = FresnelSchlick(NdotH, Reflectivity);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL;

    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0 - bsdfInput.TransmissionWeight);
    kD *= kD * (vec3(1.0) - kS);
    kD *= 1.0 - bsdfInput.InterfaceMetallic;

    vec3 Lo = (kD * bsdfInput.BaseColor / MATH_PI + specular) * NdotL;

    return Lo;
}

vec3 CookTorranceBTDF(in BSDF_Input bsdfInput)
{
#if _DEBUG
    if(dot(L, N) < 0.0)
        return vec3(1.0, 0.0, 0.0);
#endif

    float RefractiveIndex = bsdfInput.RefractiveIndexRatio;

    vec3 T = refract(-bsdfInput.LightDir, -bsdfInput.HalfVec, RefractiveIndex);

    if(length(T) == 0.0)
        return vec3(0, 0, 0);

    if (RefractiveIndex == 1.0)
        return vec3(1.0);

    T = normalize(T);

    float NdotH = max(dot(bsdfInput.Normal, bsdfInput.HalfVec), SHADING_TOLERANCE);
    float NdotL = max(dot(-bsdfInput.Normal, bsdfInput.LightDir), SHADING_TOLERANCE);
    float NdotT = dot(-bsdfInput.Normal, T);

    float LdotH = max(dot(bsdfInput.LightDir, -bsdfInput.HalfVec), SHADING_TOLERANCE);
    float TdotH = dot(T, -bsdfInput.HalfVec);

    vec3 Reflectivity = vec3((RefractiveIndex - 1.0) / (RefractiveIndex + 1.0));
    Reflectivity *= Reflectivity;

    Reflectivity = mix(Reflectivity, bsdfInput.BaseColor, bsdfInput.InterfaceMetallic);

    float NDF = DistributionGGX(NdotH, bsdfInput.InterfaceRoughness);
    float G2 = GeometrySmith(abs(NdotT), NdotL, bsdfInput.InterfaceRoughness);

    vec3 F = FresnelSchlick(NdotH, Reflectivity);

    vec3 R = (vec3(1.0) - F) * bsdfInput.TransmissionWeight;

    R *= 1.0 - bsdfInput.InterfaceMetallic;

    vec3 kD = R;
    kD *= 1.0 - bsdfInput.TransmissionWeight;

    vec3 numerator = LdotH * NDF * G2 * R * RefractionJacobian(LdotH, TdotH, 1.0 / RefractiveIndex);
    float denominator = max(abs(NdotT) * NdotL, SHADING_TOLERANCE);

    vec3 radiance = numerator / denominator;

    vec3 Lo = (kD * bsdfInput.BaseColor / MATH_PI + radiance) * NdotL;

    //Lo = radiance * NdotL;

    return Lo;
}

vec3 CookTorranceBSDF(in BSDF_Input bsdfInput, float alpha)
{
    return mix(CookTorranceBRDF(bsdfInput), CookTorranceBTDF(bsdfInput), alpha);
}

#endif