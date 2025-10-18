#ifndef COMMON_GLSL
#define COMMON_GLSL

struct Ray
{
	vec3 Origin;
	uint MaterialIndex;
	vec3 Direction;
	uint Active; // Zero means active; any other means inactive
};

struct RayInfo
{
	uvec2 ImageCoordinate;
	vec4 Luminance;
	vec4 Throughput;
};

struct Material
{
	vec3 Albedo;
	float Metallic;
	float Roughness;
	float Transmission;
	float Padding1;
	float RefractiveIndex;
};

struct LightProperties
{
	vec3 Color;
};

struct MeshInfo
{
	uint BeginIndex;
	uint Padding;
	uint EndIndex;
	uint MaterialIndex;
};

struct LightInfo
{
	uint BeginIndex;
	uint Padding;
	uint EndIndex;
	uint LightPropsIndex;
};

struct Node
{
	vec3 MinBound;
	uint BeginIndex;

	vec3 MaxBound;
	uint EndIndex;

	uint FirstChildIndex;
	uint Padding;
	uint SecondChildIndex;
};

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

struct RayRef
{
	uint MaterialIndex;
	uint FieldIndex;
};

vec3 GetPoint(Ray ray, float Par)
{
	return ray.Origin + ray.Direction * Par;
}

#endif
