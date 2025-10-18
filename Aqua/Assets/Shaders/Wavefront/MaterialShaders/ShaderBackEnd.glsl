// Shader back end of the material pipeline
// Uses the place holder shader: SampleInfo Evaluate(in Ray, in CollisionInfo);

// Post processing for cube map texture
vec3 GammaCorrectionInv(in vec3 color)
{
	return pow(color, vec3(2.2));
}

float NormalizedMagnitude(in vec3 throughput)
{
	// Max value of the magnitude is assumed to be sqrt(3) for vec3(1, 1, 1)
	return length(throughput) / sqrt(3);
}

float MaxComponent(in vec3 vec)
{
	return max(vec.x, max(vec.y, vec.z));
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

// russian roulette
bool ApplyRussianRoulette(inout SampleInfo sampleInfo, inout Ray ray, in vec3 throughput)
{
	float cutoff = NormalizedMagnitude(throughput);

	// prevent the throughput from dropping too much
	cutoff = max(uThroughputFloor, cutoff);

	float Xi = GetRandom(sRandomSeed);
	
	if (Xi > cutoff)
	{
		ray.Active = RR_CUTOFF_CONST;
		return false;
	}
	
	sampleInfo.Luminance /= cutoff;

	return true;
}

// Light shader
SampleInfo EvaluateLightShader(in Ray ray, in CollisionInfo collisionInfo)
{
	SampleInfo sampleInfo;

	sampleInfo.Direction = vec3(0.0);
	sampleInfo.iNormal = vec3(0.0);
	sampleInfo.IsInvalid = false;
	sampleInfo.Weight = 1.0;
	sampleInfo.SurfaceNormal = vec3(0.0);
	sampleInfo.Throughput = vec3(1.0);

	sampleInfo.Luminance = sLightPropsInfos[sLightInfos[collisionInfo.MaterialIndex].LightPropsIndex].Color;

	return sampleInfo;
}

SampleInfo EvokeShader(in Ray ray, in CollisionInfo collisionInfo, in uint materialRef)
{
	// TODO: Can be optimized...
	switch (materialRef)
	{
		case EMPTY_MATERIAL_ID:
			SampleInfo emptySample;
			emptySample.Weight = 1.0;
			emptySample.Luminance = vec3(0.0);
			emptySample.IsInvalid = false;
			emptySample.Throughput = vec3(uThroughputFloor);

			return emptySample;

		case LIGHT_MATERIAL_ID:
			// calculate the light shader here...
			return EvaluateLightShader(ray, collisionInfo);

		case SKYBOX_MATERIAL_ID:
			// TODO: calculate the skybox shader here...
			SampleInfo skyboxInfo;
			skyboxInfo.Weight = 1.0;
			skyboxInfo.IsInvalid = false;
			skyboxInfo.Throughput = vec3(1.0);

			if (uSkyboxExists != 0)
				skyboxInfo.Luminance = SampleCubeMap(ray.Direction, uSkyboxColor.w);
				//skyboxInfo.Luminance = vec3(1.0, 0.0, 0.0);
			else
				skyboxInfo.Luminance = uSkyboxColor.rgb;

			return skyboxInfo;

		default:
			// Calling the placeholder function: SampleInfo Evaluate(in Ray, in CollisionInfo);
			return Evaluate(ray, collisionInfo);
	}
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= uRayCount)
		return;

	if (uRayCount == 0)
		return;

	CollisionInfo collisionInfo = sCollisionInfos[GetActiveIndex(GlobalIdx)];
	RayInfo rayInfo = sRayInfos[GetActiveIndex(GlobalIdx)];
	Ray ray = sRays[GetActiveIndex(GlobalIdx)];

	// make sure that we're dealing with active rays
	if(ray.Active != 0)
		return;

	// Initializing the random numbers
	sRandomSeed = HashCombine(GlobalIdx, pRandomSeed);

	if (sRandomSeed == 0)
		sRandomSeed = 0x9e3770b9;

	// Dispatch the correct material here, and don't process the inactive rays
	uint MaterialRef = ray.MaterialIndex;

	bool MaterialPass = (MaterialRef == pMaterialRef);

	bool InactivePass = (MaterialRef == LIGHT_MATERIAL_ID ||
		MaterialRef == EMPTY_MATERIAL_ID ||
		MaterialRef == SKYBOX_MATERIAL_ID) &&
		(pMaterialRef == -1);

	if (!MaterialPass && !InactivePass)
		return;

	// Sampling and dispatching to the approapriate shader
	SampleInfo sampleInfo;
	sampleInfo = EvokeShader(ray, collisionInfo, MaterialRef);

	// enforce the russian roulette constraints
	if (pBounceCount < 4)
		sampleInfo.Throughput = vec3(1.0);

	// insert the luminance throughput
	rayInfo.Throughput.xyz *= sampleInfo.Throughput;
	sRayInfos[GetActiveIndex(GlobalIdx)].Throughput = rayInfo.Throughput;

	// apply russian roulette
	ApplyRussianRoulette(sampleInfo, sRays[GetActiveIndex(GlobalIdx)], rayInfo.Throughput.xyz);

	// Update the color values and ray directions
	if(!sampleInfo.IsInvalid)
		sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.Luminance * sampleInfo.Weight, 1.0);
		//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.Luminance, 1.0);
		//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.iNormal * sampleInfo.Weight, 1.0);
		//sRayInfos[GetActiveIndex(GlobalIdx)].Luminance *= vec4(sampleInfo.iNormal, 1.0);
	else
		sRayInfos[GetActiveIndex(GlobalIdx)].Luminance = vec4(0.0, 0.0, 0.0, 1.0);

	float sign = sampleInfo.IsReflected ? 1.0 : -1.0;

	// Aim at the sampled direction
	sRays[GetActiveIndex(GlobalIdx)].Origin =
		collisionInfo.IntersectionPoint + sign * collisionInfo.Normal * SHADING_TOLERANCE;

	sRays[GetActiveIndex(GlobalIdx)].Direction = sampleInfo.Direction;

	// Figure out if the ray went into a terminating material
	if (collisionInfo.HitOccured)
		sRays[GetActiveIndex(GlobalIdx)].Active = collisionInfo.IsLightSrc ? LIGHT_MATERIAL_ID : 0;
	else
		sRays[GetActiveIndex(GlobalIdx)].Active = SKYBOX_MATERIAL_ID;

	if (sampleInfo.IsInvalid)
		sRays[GetActiveIndex(GlobalIdx)].Active = EMPTY_MATERIAL_ID;
}
