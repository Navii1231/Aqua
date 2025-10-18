// Specular BSDF input structure
struct GlossyBSDFInput
{
    vec3 LightDir;           // Light direction vector
    vec3 ViewDir;            // View direction vector
    vec3 Normal;             // Surface normal vector
    vec3 BaseColor;          // Base color tint of the specular reflection
    float Roughness;         // Surface roughness for microfacet distribution
};


// Specular BSDF using the Cook-Torrance microfacet model
vec3 GlossyBSDF(in GlossyBSDFInput bsdfInput)
{
    vec3 iNormal = normalize(bsdfInput.ViewDir + bsdfInput.LightDir);
    vec3 ViewDir = bsdfInput.ViewDir;

    float NdotH = max(dot(bsdfInput.Normal, iNormal), SHADING_TOLERANCE);
    float NdotV = max(dot(bsdfInput.Normal, ViewDir), SHADING_TOLERANCE);
    float NdotL = max(dot(bsdfInput.Normal, LightDir), SHADING_TOLERANCE);

    float NDF = DistributionGGX(NdotH, bsdfInput.Roughness);
    float G = GeometrySmith(NdotL, NdotV, bsdfInput.Roughness);
    vec3 F = FresnelSchlick(NdotH, bsdfInput.BaseColor);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL;

    vec3 specular = numerator / denominator;

    vec3 Luminance = specular * NdotL;

    return Luminance;
}
