#version 440

layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = WORKGROUP_SIZE_Y) in;

layout(set = 0, binding = 0, rgba8) uniform image2D uImageOutput;

layout(push_constant) uniform ShaderData
{
	uint pImageX;
	uint pImageY;
	uint pPostProcessKey;
	uint pExposure;
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

// Apply method...

void ApplyPostProcess(inout vec3 Color)
{
	if ((pPostProcessKey & APPLY_TONE_MAP) != 0)
		Color = ToneMap(Color, pExposure);

	if ((pPostProcessKey & APPLY_GAMMA_CORRECTION) != 0)
		Color = GammaCorrection(Color);

	if ((pPostProcessKey & APPLY_GAMMA_CORRECTION_INV) != 0)
		Color = GammaCorrectionInv(Color);
}

void main()
{
	uvec2 Position = gl_GlobalInvocationID.xy;

	if (Position.x >= pImageX || Position.y >= pImageY)
		return;

	vec3 Color = imageLoad(uImageOutput, ivec2(Position)).rgb;

	ApplyPostProcess(Color);

	Color = vec3(0.2);

	imageStore(uImageOutput, ivec2(Position), vec4(Color, 1.0));
}
