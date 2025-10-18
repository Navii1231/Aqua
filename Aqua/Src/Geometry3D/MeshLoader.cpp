#include "Core/Aqpch.h"
#include "Geometry3D/MeshLoader.h"

AQUA_BEGIN

void TraverseSceneTree(aiNode* node, const aiScene* scene,
	GeometryNode* ParentNode);

glm::mat4 Convert(const ::aiMatrix4x4& matrix);
glm::vec4 Convert(const ::aiColor4D& Color);
glm::vec3 Convert(const ::aiColor3D& Color);

aiTextureType GetTextureType(TextureType type);

void TraverseSceneTree(aiNode* node, const aiScene* scene,
	GeometryNode* MyNode)
{
	// Do Pre-Traversal visit to the scene hierarchy

	MyNode->Name = node->mName.C_Str();
	MyNode->Transformation = Convert(node->mTransformation);

	MyNode->MeshRefs.resize(node->mNumMeshes);
	
	for (size_t i = 0; i < node->mNumMeshes; i++)
	{
		MyNode->MeshRefs[i] = node->mMeshes[i];
	}

	MyNode->Children.resize(node->mNumChildren);

	for (uint32_t i = 0; i < node->mNumChildren; i++)
	{
		GeometryNode* ChildNode = new GeometryNode;
		MyNode->Children[i] = ChildNode;
		MyNode->Parent = MyNode;
		TraverseSceneTree(node->mChildren[i], scene, ChildNode);
	}
}

AQUA_END

AQUA_NAMESPACE::Geometry3D AQUA_NAMESPACE::MeshLoader::LoadModel(const std::string& filepath) const
{
	Assimp::Importer importer;
	Geometry3D MyScene;

	importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 85.0f);

	// TODO: For now, we're only allowing triangles as the primitive faces
	const aiScene* scene = importer.ReadFile(filepath, mConfig.PostProcessFlag | aiProcess_Triangulate);

	if (!scene)
		return MyScene;

	AssignMeshes(MyScene.mMeshes, scene->mMeshes, scene->mNumMeshes);
	AssignMaterials(MyScene.mMaterials, scene->mMaterials, scene->mNumMaterials);

	MyScene.mRootNode = new GeometryNode;

	TraverseSceneTree(scene->mRootNode, scene, MyScene.mRootNode);

	importer.FreeScene();

	return MyScene;
}

AQUA_NAMESPACE::FlatMaterialPars AQUA_NAMESPACE::MeshLoader::GetFlatMaterialParameters(aiMaterial* OtherMat) const
{
	FlatMaterialPars Pars;

	aiColor3D Color3D;
	aiColor4D Color4D;
	float Property = 0.0f;

#define ASSIGN_FLAT_VEC(MaterialProperty, PropertyRef, PropertyAssign) \
	if(OtherMat->Get(MaterialProperty, PropertyRef) == aiReturn_SUCCESS)\
	{ Pars.PropertyAssign = Convert(PropertyRef); }

#define ASSIGN_FLAT(MaterialProperty, PropertyRef, PropertyAssign) \
	if(OtherMat->Get(MaterialProperty, PropertyRef) == aiReturn_SUCCESS)\
	{ Pars.PropertyAssign = PropertyRef; }

	ASSIGN_FLAT_VEC(AI_MATKEY_COLOR_DIFFUSE, Color4D, DiffuseColor);
	ASSIGN_FLAT_VEC(AI_MATKEY_COLOR_AMBIENT, Color3D, AmbientColor);
	ASSIGN_FLAT_VEC(AI_MATKEY_COLOR_EMISSIVE, Color3D, EmissiveColor);

	ASSIGN_FLAT(AI_MATKEY_METALLIC_FACTOR, Property, Metallicness);
	ASSIGN_FLAT(AI_MATKEY_ROUGHNESS_FACTOR, Property, Roughness);

	return Pars;
}

void AQUA_NAMESPACE::MeshLoader::AssignMaterials(std::vector<MaterialInfo>& Materials,
	aiMaterial** mMaterials, uint32_t mNumMaterials) const
{
	Materials.clear();
	Materials.resize(mNumMaterials);

	for (uint32_t i = 0; i < mNumMaterials; i++)
	{
		auto& MyMat = Materials[i];
		auto* OtherMat = mMaterials[i];

		aiString Name;

		if (OtherMat->Get(AI_MATKEY_NAME, Name) == aiReturn_SUCCESS)
			MyMat.Name = Name.C_Str();

		MyMat.FlatMaterialVals = GetFlatMaterialParameters(OtherMat);

		FillTextures(MyMat, OtherMat);
	}
}

void AQUA_NAMESPACE::MeshLoader::AssignMeshes(std::vector<MeshData>& MyMeshes,
	aiMesh** OtherMeshes, uint32_t count) const
{
	MyMeshes.clear();
	MyMeshes.resize(count);

	for (uint32_t i = 0; i < count; i++)
	{
		auto& MyMesh = MyMeshes[i];
		auto* OtherMesh = OtherMeshes[i];

		MyMesh.AssignPositions((glm::vec3*) OtherMesh->mVertices, OtherMesh->mNumVertices);

		MyMesh.AssignNormals((glm::vec3*) OtherMesh->mNormals, 
			OtherMesh->HasNormals() * OtherMesh->mNumVertices);

		MyMesh.AssignTexCoords((glm::vec3*) OtherMesh->mTextureCoords[0], 
			OtherMesh->HasNormals() * OtherMesh->mNumVertices);
		
		MyMesh.AssignTangentsAndBitangents((glm::vec3*) OtherMesh->mTangents,
			(glm::vec3*) OtherMesh->mBitangents, OtherMesh->HasTangentsAndBitangents() * OtherMesh->mNumVertices);

		MyMesh.AssignFaces((aiFace*) OtherMesh->mFaces, OtherMesh->mMaterialIndex,
			OtherMesh->mNumFaces, OtherMesh->mPrimitiveTypes);
	}
}

void AQUA_NAMESPACE::MeshLoader::FillTextures(MaterialInfo& MyMat, aiMaterial* OtherMat) const
{
	auto RetrieveTexture = [&MyMat, OtherMat](TextureType type)
	{
		auto Count = OtherMat->GetTextureCount(GetTextureType(type));

		if (Count == 0)
			return;

		aiString Filepath;
		auto result = OtherMat->GetTexture(GetTextureType(type), 0, &Filepath);

		if (result == aiReturn_SUCCESS)
		{
			MyMat.TextureFilepaths[type] = Filepath.C_Str();
		}
	};

	RetrieveTexture(TextureType::eAmbient);
	RetrieveTexture(TextureType::eDiffuse);
	RetrieveTexture(TextureType::eNormal);
	RetrieveTexture(TextureType::eDisplacement);
	RetrieveTexture(TextureType::eEmissive);
	RetrieveTexture(TextureType::eHeight);
	RetrieveTexture(TextureType::eLight);
	RetrieveTexture(TextureType::eMetallic);
	RetrieveTexture(TextureType::eRoughness);
	RetrieveTexture(TextureType::eAmbientOcclusion);
}

inline glm::mat4 AQUA_NAMESPACE::Convert(const aiMatrix4x4& matrix)
{
	glm::mat4 result;

	result[0][0] = matrix[0][0];
	result[0][1] = matrix[0][1];
	result[0][2] = matrix[0][2];
	result[0][3] = matrix[0][3];

	result[1][0] = matrix[1][0];
	result[1][1] = matrix[1][1];
	result[1][2] = matrix[1][2];
	result[1][3] = matrix[1][3];

	result[2][0] = matrix[2][0];
	result[2][1] = matrix[2][1];
	result[2][2] = matrix[2][2];
	result[2][3] = matrix[2][3];

	result[3][0] = matrix[3][0];
	result[3][1] = matrix[3][1];
	result[3][2] = matrix[3][2];
	result[3][3] = matrix[3][3];

	return result;
}

glm::vec4 AQUA_NAMESPACE::Convert(const aiColor4D& Color)
{
	return { Color.r, Color.g, Color.b, Color.a };
}

glm::vec3 AQUA_NAMESPACE::Convert(const aiColor3D& Color)
{
	return { Color.r, Color.g, Color.b };
}

aiTextureType AQUA_NAMESPACE::GetTextureType(TextureType type)
{
	switch (type)
	{
		case TextureType::eNone:			       return aiTextureType_NONE;
		case TextureType::eDiffuse:			       return aiTextureType_DIFFUSE;
		case TextureType::eAmbient:			       return aiTextureType_AMBIENT;
		case TextureType::eEmissive:		       return aiTextureType_EMISSIVE;
		case TextureType::eHeight:			       return aiTextureType_HEIGHT;
		case TextureType::eNormal:			       return aiTextureType_NORMALS;
		case TextureType::eDisplacement:	       return aiTextureType_DISPLACEMENT;
		case TextureType::eLight:		           return aiTextureType_LIGHTMAP;
		case TextureType::eMetallic:		       return aiTextureType_METALNESS;
		case TextureType::eRoughness:              return aiTextureType_DIFFUSE_ROUGHNESS;
		case TextureType::eAmbientOcclusion:       return aiTextureType_AMBIENT_OCCLUSION;
		case TextureType::eUnknown:			       return aiTextureType_UNKNOWN;
		default:                                   return aiTextureType_UNKNOWN;

	}
}
