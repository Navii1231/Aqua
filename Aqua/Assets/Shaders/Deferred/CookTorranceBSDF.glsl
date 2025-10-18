struct CookTorranceBSDFInput
{
    vec3 ViewDir;
    float Roughness;

    vec3 Normal;
    float Metallic;

    vec3 BaseColor;
    float RefractiveIndex;

	vec3 LightDir;
    float TransmissionWeight;
};

vec3 CookTorranceBRDF(in CookTorranceBSDFInput bsdfInput)
{
	vec3 ViewDir = bsdfInput.ViewDir;
    vec3 iNormal = normalize(bsdfInput.ViewDir + bsdfInput.LightDir);

    float NdotH = max(dot(bsdfInput.Normal, iNormal), SHADING_TOLERANCE);
    float NdotV = max(dot(bsdfInput.Normal, ViewDir), SHADING_TOLERANCE);
    float NdotL = max(dot(bsdfInput.Normal, bsdfInput.LightDir), SHADING_TOLERANCE);
	float HdotV = max(dot(iNormal, ViewDir), SHADING_TOLERANCE);

    vec3 Reflectivity = vec3((bsdfInput.RefractiveIndex - 1.0) /
        (bsdfInput.RefractiveIndex + 1.0));

    Reflectivity *= Reflectivity;

    Reflectivity = mix(Reflectivity, bsdfInput.BaseColor, bsdfInput.Metallic);

    float NDF = DistributionGGX(NdotH, bsdfInput.Roughness);
    float G = GeometrySmith(NdotL, NdotV, bsdfInput.Roughness);
    vec3 F = FresnelSchlick(NdotH, Reflectivity);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL;

    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0 - bsdfInput.TransmissionWeight);
    kD *= kD * (vec3(1.0) - kS);
    kD *= 1.0 - bsdfInput.Metallic;

    vec3 Lo = (kD * bsdfInput.BaseColor / MATH_PI + specular) * NdotL;
    //Lo = specular * NdotL;
    //Lo = (kD * bsdfInput.BaseColor / MATH_PI) * NdotL;

	return Lo;
}
