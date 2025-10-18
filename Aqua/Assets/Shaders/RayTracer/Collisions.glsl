#ifndef COLLISIONS_GLSL
#define COLLISIONS_GLSL

#include "RayTracerConfig.glsl"

const float FLT_MAX = 3.4028235e+38;
const float TOLERANCE = 0.00001;

struct CollisionInfo
{
	// Values set by the collision solver...
	vec3 Normal;
	float RayDis;
	vec3 IntersectionPoint;
	float NormalInverted;

	// Intersection primitive info
	vec3 bCoords;
	uint PrimitiveID;

	// Value set by the collider...
	uint MaterialIndex;

	// Booleans...
	bool HitOccured;
	bool IsLightSrc;
};

struct AABB_CollisionInfo
{
	bool HitOccured;
	float RayDis;
};

void SwapWithCondition(inout float a, inout float b, bool condition)
{
	float Temp = b;
	b = condition ? a : b;
	a = condition ? Temp : a;
}

vec3 InterpolateNormal(in vec3 bCoords, uint PrimitiveID)
{
	uvec4 Face = sIndexBuffer.Indices[PrimitiveID];

	return normalize(bCoords.z * sNormalBuffer.Normals[Face.x] +
		bCoords.x * sNormalBuffer.Normals[Face.y] +
		bCoords.y * sNormalBuffer.Normals[Face.z]);
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

	bCoords.x = dot(T, H) * DeterminantInv;
	bCoords.y = dot(ray.Direction, Q) * DeterminantInv;
	bCoords.z = 1.0 - bCoords.x - bCoords.y;

	// Prepare the HitInfo buffer
	hitInfo.bCoords = bCoords;
	hitInfo.IntersectionPoint = ray.Origin + Alpha * ray.Direction;
	hitInfo.Normal = Normal;
	hitInfo.RayDis = Alpha;

	hitInfo.HitOccured = (bCoords.x >= 0.0 && bCoords.y >= 0.0 && bCoords.z >= 0.0) &&
		Alpha > TOLERANCE && abs(Determinant) > TOLERANCE;

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

	uint NodeStackIndices[64];
	uint StackPtr = 0;

	NodeStackIndices[StackPtr++] = rootIndex;

	while (StackPtr != 0)
	{
		uint CurrentIndex = NodeStackIndices[--StackPtr];

		CheckRayAABB_Collision(hitInfoAABB, ray,
			sNodeBuffer.Nodes[CurrentIndex].MinBound, sNodeBuffer.Nodes[CurrentIndex].MaxBound);

		NodeStackIndices[StackPtr++] = sNodeBuffer.Nodes[CurrentIndex].FirstChildIndex;
		NodeStackIndices[StackPtr++] = sNodeBuffer.Nodes[CurrentIndex].SecondChildIndex;

		bool Revert = !hitInfoAABB.HitOccured || hitInfoAABB.RayDis > ClosestHit.RayDis ||
			sNodeBuffer.Nodes[CurrentIndex].FirstChildIndex == rootIndex;

		StackPtr -= Revert ? 2 : 0;

		if (sNodeBuffer.Nodes[CurrentIndex].FirstChildIndex == rootIndex && hitInfoAABB.HitOccured)
		{
			for (uint j = sNodeBuffer.Nodes[CurrentIndex].BeginIndex;
				j < sNodeBuffer.Nodes[CurrentIndex].EndIndex; j++)
			{
				CheckRayTriangleCollision(hitInfo, ray, 
					sVertexBuffer.Positions[sIndexBuffer.Indices[j].x],
					sVertexBuffer.Positions[sIndexBuffer.Indices[j].y],
					sVertexBuffer.Positions[sIndexBuffer.Indices[j].z]);

				hitInfo.PrimitiveID = j;

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
		bool FoundCloser = FindCollisionNode(ClosestHit, ray, sMeshInfos.Infos[i].BeginIndex);
		ClosestHit.IsLightSrc = ClosestHit.IsLightSrc && (!FoundCloser);
		ClosestHit.MaterialIndex = FoundCloser ? sMeshInfos.Infos[i].MaterialIndex :
			ClosestHit.MaterialIndex;
	}
}

void TestRayLightCollisions(inout CollisionInfo ClosestHit, in Ray ray)
{
	// For all the light sources...

	for (uint i = 0; i < uSceneInfo.LightCount; i++)
	{
		bool FoundCloser = FindCollisionNode(ClosestHit, ray, sLightInfos.Infos[i].BeginIndex);
		ClosestHit.IsLightSrc = ClosestHit.IsLightSrc || FoundCloser;
		ClosestHit.MaterialIndex = FoundCloser ? sLightInfos.Infos[i].LightPropsIndex : ClosestHit.MaterialIndex;
	}
}

void CheckForRayCollisions(inout CollisionInfo ClosestHit, in Ray ray)
{
	ClosestHit.HitOccured = false;
	ClosestHit.RayDis = FLT_MAX;

	TestRayLightCollisions(ClosestHit, ray);
	TestRayMeshCollisions(ClosestHit, ray);
}


#endif