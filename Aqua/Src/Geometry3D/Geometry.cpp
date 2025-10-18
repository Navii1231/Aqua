#include "Core/Aqpch.h"
#include "Geometry3D/Geometry.h"
#include "Assimp/Scene.h"

AQUA_NAMESPACE::Geometry3D::~Geometry3D()
{
	DeleteNodeRecursive(mRootNode);

	MakeHollow();
}

void AQUA_NAMESPACE::Geometry3D::CopyNodeRecursive(GeometryNode* MyNode, GeometryNode* OtherNode)
{
	*MyNode = *OtherNode;

	for (size_t i = 0; i < OtherNode->Children.size(); i++)
	{
		MyNode->Children[i] = new GeometryNode;
		CopyNodeRecursive(MyNode->Children[i], OtherNode->Children[i]);
		MyNode->Children[i]->Parent = MyNode;
	}
}

AQUA_NAMESPACE::Geometry3D::Geometry3D(const Geometry3D& Other)
{
	if (!Other.mRootNode)
		return;

	mRootNode = new GeometryNode;

	CopyNodeRecursive(mRootNode, Other.mRootNode);

	mMeshes = Other.mMeshes;
	mMaterials = Other.mMaterials;
}

AQUA_NAMESPACE::Geometry3D::Geometry3D(Geometry3D&& Other) noexcept
{
	mRootNode = Other.mRootNode;
	mMeshes = std::move(Other.mMeshes);
	mMaterials = std::move(Other.mMaterials);

	Other.MakeHollow();
}

AQUA_NAMESPACE::Geometry3D& AQUA_NAMESPACE::Geometry3D::operator=(const Geometry3D& Other)
{
	DeleteNodeRecursive(mRootNode);

	if (!Other.mRootNode)
		return *this;

	mRootNode = new GeometryNode;

	CopyNodeRecursive(mRootNode, Other.mRootNode);

	mMeshes = Other.mMeshes;
	mMaterials = Other.mMaterials;

	return *this;
}

AQUA_NAMESPACE::Geometry3D& AQUA_NAMESPACE::Geometry3D::operator=(Geometry3D&& Other) noexcept
{
	DeleteNodeRecursive(mRootNode);

	mRootNode = Other.mRootNode;

	mMeshes = std::move(Other.mMeshes);
	mMaterials = std::move(Other.mMaterials);

	Other.MakeHollow();

	return *this;
}

void AQUA_NAMESPACE::Geometry3D::DeleteNodeRecursive(GeometryNode* node)
{
	if (!node) return;

	// Post order traversal for deletion
	for (auto child : node->Children)
		DeleteNodeRecursive(child);

	delete node;
}

void AQUA_NAMESPACE::Geometry3D::MakeHollow()
{
	mRootNode = nullptr;

	mMeshes.clear();
	mMaterials.clear();
}

uint32_t AQUA_NAMESPACE::MeshData::GetFaceIndexCount() const
{
	switch (mPrimitive)
	{
		case FacePrimitive::ePoint: return 1;
		case FacePrimitive::eLine: return 2;
		case FacePrimitive::eTriangle: return 3;
		case FacePrimitive::eQuad: return 4;
		default: return 0;
	}
}

size_t AQUA_NAMESPACE::MeshData::GetIndexCount() const
{
	return aFaces.size() * GetFaceIndexCount();
}

void AQUA_NAMESPACE::MeshData::AssignPositions(const glm::vec3* positions, uint32_t count)
{
	aPositions.clear();

	aPositions.resize(count);

	for (uint32_t i = 0; i < count; i++)
	{
		aPositions[i] = positions[i];
	}
}

void AQUA_NAMESPACE::MeshData::AssignTexCoords(const glm::vec3* coords, uint32_t count)
{
	aTexCoords.clear();

	aTexCoords.resize(count);

	for (uint32_t i = 0; i < count; i++)
	{
		aTexCoords[i] = coords[i];
	}
}

void AQUA_NAMESPACE::MeshData::AssignNormals(const glm::vec3* normals, uint32_t count)
{
	aNormals.clear();

	aNormals.resize(count);

	for (uint32_t i = 0; i < count; i++)
	{
		aNormals[i] = normals[i];
	}
}

void AQUA_NAMESPACE::MeshData::AssignTangentsAndBitangents(
	const glm::vec3* tangents, const glm::vec3* bitangents, uint32_t count)
{
	aTangents.clear();
	aBitangents.clear();

	aTangents.resize(count);
	aBitangents.resize(count);

	for (uint32_t i = 0; i < count; i++)
	{
		aTangents[i] = tangents[i];
		aBitangents[i] = bitangents[i];
	}
}

void AQUA_NAMESPACE::MeshData::AssignFaces(const aiFace* faces, uint32_t materialRef, 
	uint32_t count, unsigned int primType)
{
	// TODO: Do any general type of face primitive...

	aFaces.clear();
	aFaces.resize(count);

	mPrimitive = ConvertPrimitiveType(primType);

	for (uint32_t i = 0; i < count; i++)
	{
		_STL_ASSERT(faces->mNumIndices == 3, "Error while loading a scene: Faces are not triangulated!");

		aFaces[i].Indices.x = faces->mIndices[0];
		aFaces[i].Indices.y = faces->mIndices[1];
		aFaces[i].Indices.z = faces->mIndices[2];
		aFaces[i].Indices.w = faces->mIndices[3];

		aFaces[i].MaterialRef = materialRef;

		faces++;
	}
}

void AQUA_NAMESPACE::MeshData::SetMaterialRef(uint32_t materialRef)
{
	SetMaterialRef(0, aFaces.size(), materialRef);
}

void AQUA_NAMESPACE::MeshData::SetMaterialRef(size_t BeginIdx, size_t EndIdx, uint32_t materialRef)
{
	std::for_each(aFaces.begin() + BeginIdx, aFaces.begin() + EndIdx, 
		[materialRef](Face& face)
	{
		face.MaterialRef = materialRef;
	});
}

AQUA_NAMESPACE::FacePrimitive AQUA_NAMESPACE::MeshData::ConvertPrimitiveType(unsigned int primType)
{
	switch ((aiPrimitiveType) primType)
	{
		// TODO: introduce a point primitive type
		case aiPrimitiveType_POINT:    return FacePrimitive::eLine;
		case aiPrimitiveType_LINE:     return FacePrimitive::eLine;
		case aiPrimitiveType_TRIANGLE: return FacePrimitive::eTriangle;
		case aiPrimitiveType_POLYGON:
		case aiPrimitiveType_NGONEncodingFlag:
		case _aiPrimitiveType_Force32Bit:
		default:
			// TODO: Need to handle this case properly
			return FacePrimitive::eTriangle;
			_STL_ASSERT(false, "Unknown primitive type");
			return FacePrimitive::ePoint;
	}
}
