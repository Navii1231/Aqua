import DiffuseBSDF
import GlossyBSDF

layout(set = 2, binding = 0) uniform sampler2D uTexture;

SampleInfo Evaluate(in Ray ray, in CollisionInfo collisionInfo)
{
	// diffuse stuff
	DiffuseBSDF_Input diffuseInput;
	diffuseInput.ViewDir = -ray.Direction;
	diffuseInput.Normal = collisionInfo.Normal;
	diffuseInput.BaseColor = vec3(0.6, 0.6, 0.6);

	Face face = sFaces[collisionInfo.PrimitiveID];

	vec2 tex = GetTexCoords(face, collisionInfo);

	diffuseInput.BaseColor = texture(uTexture, tex).rgb;
	//diffuseInput.BaseColor = vec3(0.6);

	// glossy stuff
	GlossyBSDF_Input glossyInput;
	glossyInput.ViewDir = -ray.Direction;
	glossyInput.Normal = collisionInfo.Normal;
	glossyInput.BaseColor = vec3(1.0, 1.0, 1.0);
	//glossyInput.BaseColor = vec3(192.0 / 256.0);
	//glossyInput.BaseColor = @vec3.base_color;
	glossyInput.Roughness = 0.0001;
	//glossyInput.Roughness = @float.roughness;

	SampleInfo finalSample;

	float Xi = GetRandom(sRandomSeed);

	if(Xi > float.DiffGlossSplit)
	{
		finalSample = SampleDiffuseBSDF(diffuseInput);
		finalSample.Luminance = DiffuseBSDF(diffuseInput, finalSample);	
	}
	else
	{
		finalSample = SampleGlossyBSDF(glossyInput);
		finalSample.Luminance = GlossyBSDF(glossyInput, finalSample);
	}

	//finalSample.Weight = 1.0;
	//finalSample.Luminance = vec3(0.6, 0.0, 0.6);

	return finalSample;
}
