#version 440

// This stage can be further optimized by using RT hardware extensions!

layout(local_size_x = WORKGROUP_SIZE) in;

#define STACK_SIZE 64

#include "DescSet0.glsl"
#include "DescSet1.glsl"

layout(push_constant) uniform RayData
{
	uint pRayCount;
	uint pActiveBuffer;
};

struct AABB_CollisionInfo
{
	bool HitOccured;
	float RayDis;
};

uint IndexOffset(uint index)
{
	return pRayCount * pActiveBuffer + index;
}

uint InverseOffset(uint index)
{
	return pRayCount * (1 - pActiveBuffer) + index;
}

void SwapWithCondition(inout float a, inout float b, bool condition)
{
	float Temp = b;
	b = condition ? a : b;
	a = condition ? Temp : a;
}

vec3 InterpolateNormal(in vec3 bCoords, uint PrimitiveID)
{
	uvec4 Face = sFaces[PrimitiveID].Indices;

	return normalize(bCoords.z * sNormals[Face.x] +
		bCoords.x * sNormals[Face.y] +
		bCoords.y * sNormals[Face.z]);
}

vec3 GetSurfaceNormal(in CollisionInfo HitInfo)
{
	//#if SMOOTH_SHADE
	//	return InterpolateNormal(HitInfo.bCoords, HitInfo.PrimitiveID);
	//#else
	//	return HitInfo.Normal;
	//#endif

	return HitInfo.Normal;

	return InterpolateNormal(HitInfo.bCoords, HitInfo.PrimitiveID) * HitInfo.NormalInverted;
}

void CheckRayTriangleCollision(inout CollisionInfo hitInfo, in Ray ray, in vec3 A, in vec3 B, in vec3 C)
{
	hitInfo.HitOccured = false;

	vec3 E1 = B - A;
	vec3 E2 = C - A;

	// Normal, Determinant and the ray dis calculation
	vec3 Normal = normalize(cross(E1, E2));

	vec3 H = cross(ray.Direction, E2);
	float Determinant = dot(E1, H);

	float DeterminantInv = 1.0 / Determinant;

	vec3 T = ray.Origin - A;
	vec3 Q = cross(T, E1);
	float Alpha = dot(E2, Q) * DeterminantInv;

	// Calculate barycentric coords
	vec3 bCoords;

	bCoords.g = dot(T, H) * DeterminantInv;
	bCoords.b = dot(ray.Direction, Q) * DeterminantInv;
	bCoords.r = 1.0 - bCoords.b - bCoords.g;

	// Prepare the HitInfo buffer
	hitInfo.bCoords = bCoords;
	hitInfo.IntersectionPoint = ray.Origin + Alpha * ray.Direction;
	hitInfo.Normal = Normal;
	hitInfo.RayDis = Alpha;

	hitInfo.HitOccured = (bCoords.x >= 0.0 && bCoords.y >= 0.0 && bCoords.z >= 0.0) &&
		Alpha > 0.0 && abs(Determinant) > TOLERANCE;

	hitInfo.NormalInverted = dot(Normal, ray.Direction) < 0 ? 1.0 : -1.0;
	hitInfo.Normal *= hitInfo.NormalInverted;
}

void CheckRayAABB_Collision(inout AABB_CollisionInfo hitInfo,
	in Ray ray, in vec3 minCorner, in vec3 maxCorner)
{
	hitInfo.HitOccured = false;

	float tMin = (minCorner.x - ray.Origin.x) / ray.Direction.x;
	float tMax = (maxCorner.x - ray.Origin.x) / ray.Direction.x;

	SwapWithCondition(tMin, tMax, tMin > tMax);

	float tyMin = (minCorner.y - ray.Origin.y) / ray.Direction.y;
	float tyMax = (maxCorner.y - ray.Origin.y) / ray.Direction.y;

	SwapWithCondition(tyMin, tyMax, tyMin > tyMax);

	float txMin = tMin;
	float txMax = tMax;

	tMin = max(tMin, tyMin);
	tMax = min(tMax, tyMax);

	float tzMin = (minCorner.z - ray.Origin.z) / ray.Direction.z;
	float tzMax = (maxCorner.z - ray.Origin.z) / ray.Direction.z;

	SwapWithCondition(tzMin, tzMax, tzMin > tzMax);

	tMin = max(tMin, tzMin);
	tMax = min(tMax, tzMax);

	hitInfo.HitOccured =
		(tMin < tzMax) && (tzMin < tMax) &&
		(tMin < tyMax) && (tyMin < tMax) &&
		((tMin < tMax) && (tMax > 0.0));

	hitInfo.RayDis = tMin > 0.0 ? tMin : 0.0;
}

bool FindCollisionNode(inout CollisionInfo ClosestHit, in Ray ray, in uint rootIndex)
{
	bool FoundCloser = false;

	CollisionInfo hitInfo;

	AABB_CollisionInfo hitInfoAABB;

	uint NodeStackIndices[STACK_SIZE];
	uint StackPtr = 0;

	NodeStackIndices[StackPtr++] = rootIndex;

	while (StackPtr != 0)
	{
		uint CurrentIndex = NodeStackIndices[--StackPtr];

		CheckRayAABB_Collision(hitInfoAABB, ray,
			sNodes[CurrentIndex].MinBound, sNodes[CurrentIndex].MaxBound);

		NodeStackIndices[StackPtr++] = sNodes[CurrentIndex].FirstChildIndex;
		NodeStackIndices[StackPtr++] = sNodes[CurrentIndex].SecondChildIndex;

		bool Revert = !hitInfoAABB.HitOccured || hitInfoAABB.RayDis > ClosestHit.RayDis ||
			sNodes[CurrentIndex].FirstChildIndex == rootIndex;

		StackPtr -= Revert ? 2 : 0;

		if (sNodes[CurrentIndex].FirstChildIndex == rootIndex && hitInfoAABB.HitOccured)
		{
			for (uint j = sNodes[CurrentIndex].BeginIndex;
				j < sNodes[CurrentIndex].EndIndex; j++)
			{
				CheckRayTriangleCollision(hitInfo, ray,
					sPositions[sFaces[j].Indices.x],
					sPositions[sFaces[j].Indices.y],
					sPositions[sFaces[j].Indices.z]);

				hitInfo.PrimitiveID = j;
				hitInfo.MaterialIndex = sFaces[j].MaterialRef;

				bool Replaced = hitInfo.HitOccured &&
					(hitInfo.RayDis < ClosestHit.RayDis);

				FoundCloser = FoundCloser || Replaced;

				ClosestHit = Replaced ? hitInfo : ClosestHit;
			}
		}
	}

	return FoundCloser;
}

void TestRayMeshCollisions(inout CollisionInfo ClosestHit, in Ray ray)
{
	// For all the mesh objects...

	for (uint i = 0; i < uSceneInfo.MeshCount; i++)
	{
		bool FoundCloser = FindCollisionNode(ClosestHit, ray, sMeshInfos[i].BeginIndex);
		ClosestHit.IsLightSrc = ClosestHit.IsLightSrc && (!FoundCloser);
	}
}

void TestRayLightCollisions(inout CollisionInfo ClosestHit, in Ray ray)
{
	// For all the light sources...

	for (uint i = 0; i < uSceneInfo.LightCount; i++)
	{
		bool FoundCloser = FindCollisionNode(ClosestHit, ray, sLightInfos[i].BeginIndex);
		ClosestHit.IsLightSrc = ClosestHit.IsLightSrc || FoundCloser;
	}
}

void CheckForRayCollisions(inout CollisionInfo ClosestHit, in Ray ray)
{
	ClosestHit.HitOccured = false;
	ClosestHit.IsLightSrc = false;

	ClosestHit.RayDis = MAX_DIS;

	TestRayLightCollisions(ClosestHit, ray);
	TestRayMeshCollisions(ClosestHit, ray);

	if (!ClosestHit.HitOccured)
		ClosestHit.MaterialIndex = -2;
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= pRayCount)
		return;

	if (sRays[IndexOffset(GlobalIdx)].Active != 0)
		return;

#if 1
	sCollisionInfos[InverseOffset(GlobalIdx)] = sCollisionInfos[IndexOffset(GlobalIdx)];
	sRays[InverseOffset(GlobalIdx)] = sRays[IndexOffset(GlobalIdx)];
#endif

	// Check for collision
	CheckForRayCollisions(sCollisionInfos[IndexOffset(GlobalIdx)], sRays[IndexOffset(GlobalIdx)]);

	// Setting the necessary markers for the next stages
	sRays[IndexOffset(GlobalIdx)].MaterialIndex =
		sCollisionInfos[IndexOffset(GlobalIdx)].IsLightSrc && 
		sCollisionInfos[IndexOffset(GlobalIdx)].HitOccured ?
		-3 : sCollisionInfos[IndexOffset(GlobalIdx)].MaterialIndex;
}