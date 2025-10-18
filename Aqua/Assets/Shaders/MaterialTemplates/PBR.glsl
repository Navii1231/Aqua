import CookTorranceBSDF

vec3 Evaluate(BSDFInput bsdfInput)
{
	CookTorranceBSDFInput cookTorrInput;
	cookTorrInput.ViewDir = bsdfInput.ViewDir;
	cookTorrInput.LightDir = bsdfInput.LightDir;
	cookTorrInput.Normal = bsdfInput.Normal;

	// shader parameters
	cookTorrInput.BaseColor = vec3.base_color;
	cookTorrInput.Metallic = float.metallic;
	cookTorrInput.Roughness = float.roughness;
	cookTorrInput.RefractiveIndex = float.refract_idx;

	// redundant parameter
	cookTorrInput.TransmissionWeight = 0.0;

	return CookTorranceBRDF(cookTorrInput);
}
