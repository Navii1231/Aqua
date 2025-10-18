#pragma once
#include "GeometryConfig.h"

AQUA_BEGIN

/**************** Next thing TODO *******************
	The materials often consist of uniquely named parameters
	tuned out and set by the artists. Thus, we need a way to
	access those random parameters from the material
	by their names. Furthermore, a material also contains
	a shader often found in terms of graph or a tree. The shader determines
	how those parameters are used and what resources
	are required to shade the object. In other words,
	we want a material system which can translate this graph into a
	parameterized shader that we can run on the GPU
* ***************************************************/

class Geometry3D
{
public:
	Geometry3D() = default;
	AQUA_API ~Geometry3D();

	AQUA_API Geometry3D(const Geometry3D&);
	AQUA_API Geometry3D(Geometry3D&&) noexcept;

	AQUA_API Geometry3D& operator =(const Geometry3D&);
	AQUA_API Geometry3D& operator =(Geometry3D&&) noexcept;

	MeshData& operator [](size_t index) { return mMeshes[index]; }
	const MeshData& operator [](size_t index) const { return mMeshes[index]; }

	auto begin() const { return mMeshes.begin(); }
	auto end() const { return mMeshes.end(); }

	const GeometryNode* GetRootNode() const { return mRootNode; }

	const std::vector<MeshData>& GetMeshData() const { return mMeshes; }
	const std::vector<MaterialInfo>& GetMaterials() const { return mMaterials; }

private:
	GeometryNode* mRootNode = nullptr;
	
	std::vector<MeshData> mMeshes;
	std::vector<MaterialInfo> mMaterials; // Limited to shaders models but doesn't work for generic materials

	friend class MeshLoader;

private:
	// Helper methods...
	void CopyNodeRecursive(GeometryNode* MyNode, GeometryNode* OtherNode);
	void DeleteNodeRecursive(GeometryNode* mRootNode);
	void MakeHollow();
};

AQUA_END
