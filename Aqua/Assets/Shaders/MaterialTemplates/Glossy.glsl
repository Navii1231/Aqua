import GlossyBSDF

vec3 Evaluate(BSDFInput bsdfInput)
{
	GlossyBSDFInput glossyInput;
	glossyInput.ViewDir = bsdfInput.ViewDir;
	glossyInput.LightDir = bsdfInput.LightDir;
	glossyInput.Normal = bsdfInput.Normal;

	// shader parameters
	glossyInput.BaseColor = vec3.base_color;
	glossyInput.Roughness = float.roughness;

	return GlossyBSDF(glossyInput);
}
