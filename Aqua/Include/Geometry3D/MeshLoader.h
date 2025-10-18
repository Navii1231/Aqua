#pragma once
#include "Geometry.h"

#include "Assimp/Importer.hpp"
#include "Assimp/postprocess.h"
#include "Assimp/Scene.h"

AQUA_BEGIN

// Loads 3D models from files (thread safe)
// 'MeshLoader' doesn't create any internal state while loading
// The instance holds only the Assimp configuration it was created with
// as a const field, making it optimal for multi threaded environments

// TODO: In this MeshData class, we need to access the shader and its parameters
class MeshLoader
{
public:
	struct Config
	{
		aiPostProcessSteps PostProcessFlag{};

		bool operator ==(const Config&) const = default;
		bool operator !=(const Config&) const = default;
	};

public:
	explicit MeshLoader(uint32_t PostProcessing)
		: mConfig({ static_cast<aiPostProcessSteps>(PostProcessing) }) {}

	~MeshLoader() = default;

	AQUA_API Geometry3D LoadModel(const std::string& filepath) const;

private:
	const Config mConfig;

private:
	// Helper Methods...
	FlatMaterialPars GetFlatMaterialParameters(aiMaterial* OtherMat) const;
	void AssignMaterials(std::vector<MaterialInfo>& Materials, aiMaterial** mMaterials, uint32_t mNumMaterials) const;
	void AssignMeshes(std::vector<MeshData>& MyMeshes, aiMesh** OtherMeshes, uint32_t count) const;
	void FillTextures(MaterialInfo& MyMat, aiMaterial* OtherMat) const;
};

AQUA_END

namespace std
{
	template <>
	struct hash<AQUA_NAMESPACE::MeshLoader::Config>
	{
		size_t operator()(const AQUA_NAMESPACE::MeshLoader::Config& config) const
		{
			return hash<uint32_t>()(config.PostProcessFlag);
		}
	};
}
