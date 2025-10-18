import DiffuseBSDF

vec3 Evaluate(BSDFInput bsdfInput)
{
	DiffuseBSDFInput diffuseInput;
	diffuseInput.ViewDir = bsdfInput.ViewDir;
	diffuseInput.LightDir = bsdfInput.LightDir;
	diffuseInput.Normal = bsdfInput.Normal;

	// shader parameters
	diffuseInput.BaseColor = vec3.base_color;

	return DiffuseBSDF(diffuseInput);
}
