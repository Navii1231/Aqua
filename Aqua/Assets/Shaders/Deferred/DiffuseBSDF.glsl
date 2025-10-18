// Diffuse BSDF input structure
struct DiffuseBSDFInput
{
    vec3 LightDir;
    vec3 ViewDir;            // View direction vector
    vec3 Normal;             // Surface Normal
    vec3 BaseColor;          // Base color of the material (albedo)
};

// Diffuse BSDF function
vec3 DiffuseBSDF(in DiffuseBSDFInput bsdfInput)
{
    vec3 iNormal = normalize(bsdfInput.ViewDir + bsdfInput.LightDir);
    return LambertianBRDF(iNormal, bsdfInput.LightDir, bsdfInput.BaseColor);
}
