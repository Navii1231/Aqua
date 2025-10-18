#version 440

layout(local_size_x = WORKGROUP_SIZE) in;

#include "DescSet0.glsl"
#include "DescSet1.glsl"

uint sRandSeed;

layout(push_constant) uniform RayData
{
	uint pRayCount;
	uint pActiveBuffer;
};

uint IndexOffset(uint index)
{
	return pRayCount * pActiveBuffer + index;
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= pRayCount)
		return;

	uint InactiveBuffer = 1 - pActiveBuffer;

	sRayRefs[GlobalIdx].FieldIndex = GlobalIdx;

	sRayRefs[GlobalIdx].MaterialIndex
		= sRays[IndexOffset(GlobalIdx)].MaterialIndex;
}
