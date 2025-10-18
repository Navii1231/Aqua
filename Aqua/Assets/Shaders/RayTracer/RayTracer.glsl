#ifndef RAY_TRACER_GLSL
#define RAY_TRACER_GLSL

#include "Sampler.glsl"

const vec3 MaxRadiance = vec3(100.0);

vec3 GlobalIllumination(in vec3[STACK_SIZE] BSDF_Stack, in vec3 LightIntensity, int Depth)
{
	// The last entry is always assumed to be a light source or a fragment on a skybox
	if (Depth < 0)
		return vec3(0.0);

	vec3 Radiance = LightIntensity;

	while (Depth > 0)
	{
		Radiance = BSDF_Stack[--Depth] * Radiance;
	}

	return Radiance;
}

vec3 TraceRayMonteCarlo(in Ray ray)
{
	// Russian roulette path termination
	float Throughput = 1.0;

	Ray CurrRay = ray;
	CollisionInfo HitInfo;
	SampleInfo sampleInfo;

	sampleInfo.IsInvalid = false;
	
	// Stack for keeping track of the BSDF values as the ray bounces around the scene
	vec3 BSDF_Stack[STACK_SIZE];
	int StackPtr = 0;

	int i = 0;

	Material ObjectMaterial;

	for(; i < uSceneInfo.MaxBounceLimit; i++)
	{
		CheckForRayCollisions(HitInfo, CurrRay);

		// Russian roulette path termination probability
		float Xi = GetRandom(sRandomGeneratorSeed);

		if (!HitInfo.HitOccured || HitInfo.IsLightSrc || sampleInfo.IsInvalid || Xi > Throughput)
			break;

		ObjectMaterial = sMaterialBuffer.Infos[HitInfo.MaterialIndex];

		float RefractiveIndex = pow(ObjectMaterial.RefractiveIndex, -HitInfo.NormalInverted);
		 
		// *** TODO: Dispatch to the shading model assigned to the object by the user ***
		// *************** For now, sampling from Cook Torrance BSDF by default *****************

		SampleUnitVec(sampleInfo, HitInfo, CurrRay.Direction, RefractiveIndex);

		BSDF_Input bsdfInput;

		bsdfInput.LightDir = sampleInfo.Direction;
		bsdfInput.InterfaceRoughness = ObjectMaterial.Roughness;
		bsdfInput.Normal = sampleInfo.SurfaceNormal;
		bsdfInput.InterfaceMetallic = ObjectMaterial.Metallic;
		bsdfInput.HalfVec = sampleInfo.iNormal;
		bsdfInput.TransmissionWeight = ObjectMaterial.Transmission;
		bsdfInput.BaseColor = ObjectMaterial.Albedo;
		bsdfInput.RefractiveIndexRatio = 1.0 / RefractiveIndex;

		BSDF_Stack[StackPtr] = CookTorranceBSDF(bsdfInput, float(!sampleInfo.IsReflected));

		// ******************************************************************************

		// Push the BSDF onto the stack
		BSDF_Stack[StackPtr] *= sampleInfo.Weight / Throughput;

		//float Threshold = 5.0;
		//
		//if (BSDF_Stack[StackPtr].r > Threshold || BSDF_Stack[StackPtr].g > Threshold || BSDF_Stack[StackPtr].b > Threshold)
		//	return vec3(1000.0, 0.0, 0.0);

		StackPtr++;

		// Russian Roulette update!
		Throughput = StackPtr > uSceneInfo.MinBounceLimit ? Throughput * sampleInfo.Throughput : 1.0;

		float Sign = (float(sampleInfo.IsReflected) - 0.5) * 2.0;

		CurrRay.Origin = HitInfo.IntersectionPoint + Sign * HitInfo.Normal * TOLERANCE;
		CurrRay.Direction = sampleInfo.Direction;
	}

	if (HitInfo.HitOccured && !HitInfo.IsLightSrc)
		return vec3(0.0);

	vec3 LightSrcIntensity = HitInfo.HitOccured ? sLightPropsBuffer.Infos[HitInfo.MaterialIndex].Color
		: SampleCubeMap(CurrRay.Direction, 0.0);

	vec3 IncomingLight = GlobalIllumination(BSDF_Stack, LightSrcIntensity, StackPtr);

	return IncomingLight;
}

#endif