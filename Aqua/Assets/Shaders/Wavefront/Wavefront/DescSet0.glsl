#ifndef DESC_SET_0_GLSL
#define DESC_SET_0_GLSL

#include "Common.glsl"

layout(set = 0, binding = 0) buffer RayBuffer
{
	Ray sRays[];
};

layout(set = 0, binding = 1) uniform CameraInfo
{
	// Physical properties (in mm)...
	vec2 SensorSize;
	float FocalLength;
	float ApertureSize;
	float FocalDistance; // in meters...
	float FOV; // in degrees...
} uCamera;

layout(set = 0, binding = 2) buffer CollisionInfoBuffer
{
	CollisionInfo sCollisionInfos[];
};

layout(set = 0, binding = 3) buffer SortingRefsBuffer
{
	RayRef sRayRefs[];
};

layout(set = 0, binding = 4) buffer RayInfoBuffer
{
	RayInfo sRayInfos[];
};

#endif
