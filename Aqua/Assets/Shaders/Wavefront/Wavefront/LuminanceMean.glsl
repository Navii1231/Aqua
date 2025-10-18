#version 440

layout(local_size_x = WORKGROUP_SIZE) in;

#include "DescSet0.glsl"
#include "DescSet1.glsl"

layout(set = 2, binding = 0, rgba8) uniform image2D uImageOutput;
layout(set = 2, binding = 1, rgba32f) uniform image2D uColorMean;
layout(set = 2, binding = 2, rgba32f) uniform image2D uColorVariance;

layout(push_constant) uniform ShaderData
{
	uint pRayCount;
	uint pActiveBuffer;
};

uint ActiveBufferIndex(uint index)
{
	return pRayCount * pActiveBuffer + index;
}

uint InactiveBufferIndex(uint index)
{
	return pRayCount * (1 - pActiveBuffer) + index;
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= pRayCount)
		return;

	uvec2 Coordinate = sRayInfos[ActiveBufferIndex(GlobalIdx)].ImageCoordinate;
	vec3 IncomingLight = sRayInfos[ActiveBufferIndex(GlobalIdx)].Luminance.rgb;

	uint activeIdx = sRays[ActiveBufferIndex(GlobalIdx)].Active;

	// Add luminances only when ray hits the skybox or a light src...
	if (activeIdx != -3 && activeIdx != -2)
		IncomingLight = vec3(0.0, 0.0, 0.0);

	vec3 ExistingColor = imageLoad(uColorMean, ivec2(Coordinate)).rgb;

	vec3 Delta = IncomingLight - ExistingColor;
	vec3 Color = ExistingColor + Delta / uSceneInfo.FrameCount;

	imageStore(uColorMean, ivec2(Coordinate), vec4(Color, 1.0));
	imageStore(uImageOutput, ivec2(Coordinate), vec4(Color, 1.0));
}
