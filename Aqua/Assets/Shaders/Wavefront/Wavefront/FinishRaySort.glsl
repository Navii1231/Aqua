#version 440

layout(local_size_x = WORKGROUP_SIZE) in;

#include "DescSet0.glsl"
#include "DescSet1.glsl"

uint sRandSeed;

layout(push_constant) uniform RayData
{
	uint pRayCount;
	uint pActiveBuffer;
	uint pRayRefActiveBuffer;
};

uint IndexOffsetActive(uint index)
{
	return pRayCount * pActiveBuffer + index;
}

uint IndexOffsetInactive(uint index)
{
	return pRayCount * (1 - pActiveBuffer) + index;
}

uint IndexOffsetRayRef(uint index)
{
	return pRayRefActiveBuffer * pRayCount + index;
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= pRayCount)
		return;
	
	uint InactiveBuffer = 1 - pActiveBuffer;

	sRays[IndexOffsetInactive(GlobalIdx)] =
		sRays[IndexOffsetActive(sRayRefs[IndexOffsetRayRef(GlobalIdx)].FieldIndex)];

	sCollisionInfos[IndexOffsetInactive(GlobalIdx)] =
		sCollisionInfos[IndexOffsetActive(sRayRefs[IndexOffsetRayRef(GlobalIdx)].FieldIndex)];

	sRayInfos[IndexOffsetInactive(GlobalIdx)] =
		sRayInfos[IndexOffsetActive(sRayRefs[IndexOffsetRayRef(GlobalIdx)].FieldIndex)];
}
