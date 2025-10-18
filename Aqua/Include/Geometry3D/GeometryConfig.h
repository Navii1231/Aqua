#pragma once
#include "../Core/AqCore.h"

struct aiFace;

AQUA_BEGIN

class Geometry3D;
struct GeometryNode;

enum class TextureType
{
	eNone                        = 0,
	eAmbient                     = 1,
	eDiffuse                     = 2,
	eNormal                      = 3,
	eHeight                      = 4,
	eDisplacement                = 5,
	eMetallic                    = 6,
	eRoughness                   = 7,
	eLight                       = 8,
	eEmissive                    = 9,
	eAmbientOcclusion            = 10,
	eUnknown,
};

enum class FacePrimitive
{
	ePoint                       = 0,
	eLine                        = 1,
	eTriangle                    = 2,
	eQuad                        = 3,
};

struct FlatMaterialPars
{
	glm::vec4 DiffuseColor = glm::vec4(1.0f);
	glm::vec3 AmbientColor = glm::vec3(0.15f);
	glm::vec3 EmissiveColor = glm::vec3(0.15f);
	float Metallicness = 0.2f;
	float Roughness = 0.6f;
};

struct MaterialInfo
{
	std::string Name;
	FlatMaterialPars FlatMaterialVals;

	std::unordered_map<TextureType, std::string> TextureFilepaths;
};

struct Face
{
	alignas(16) glm::uvec4 Indices;

	alignas(4) uint32_t MaterialRef;
	alignas(4) uint32_t Padding1;

	alignas(4) uint32_t FaceID;
	alignas(4) uint32_t Padding2;
};

struct MeshData
{
	std::vector<glm::vec3> aPositions;
	std::vector<glm::vec3> aTexCoords;
	std::vector<glm::vec3> aNormals;
	std::vector<glm::vec3> aTangents;
	std::vector<glm::vec3> aBitangents;

	std::vector<Face> aFaces;

	FacePrimitive mPrimitive = FacePrimitive::eTriangle;

	uint32_t GetFaceIndexCount() const;
	size_t GetIndexCount() const;
	size_t GetVertexCount() const { return aPositions.size(); }

	void AssignPositions(const glm::vec3* positions, uint32_t count);
	void AssignTexCoords(const glm::vec3* coords, uint32_t count);
	void AssignNormals(const glm::vec3* normals, uint32_t count);
	void AssignTangentsAndBitangents(const glm::vec3* tangents, const glm::vec3* bitangents, uint32_t count);

	void AssignFaces(const aiFace* faces, uint32_t materialRef, uint32_t count, unsigned int primType);

	void SetMaterialRef(uint32_t materialRef);
	void SetMaterialRef(size_t BeginIdx, size_t EndIdx, uint32_t materialRef);

private:
	static FacePrimitive ConvertPrimitiveType(unsigned int primType);
};

struct GeometryNode
{
	std::string Name;
	glm::mat4 Transformation;
	std::vector<size_t> MeshRefs;

	GeometryNode* Parent = nullptr;

	std::vector<GeometryNode*> Children;
};

AQUA_END
