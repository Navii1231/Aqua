#pragma once
#include "MaterialConfig.h"
#include "../Execution/Graph.h"

AQUA_BEGIN

enum class ShaderParError
{
	eParameterDoesntExist          = 1,
	eSizeMismatch                  = 2,
};

struct MaterialInstanceInfo
{
	MAT_NAMESPACE::ResourceMap Resources;
	MAT_NAMESPACE::ShaderParameterSet ShaderParameters;
	vkLib::DescriptorLocation ParameterLocation;
	uint32_t Stride = 0;
	MAT_NAMESPACE::Platform RendererType;
};

using MaterialInstanceInfoRef = SharedRef<MaterialInstanceInfo>;

// To ease ourselves at setting up the resources
// #set 0 and #set 1 are reserved across all shading platforms
// so assign anything to them at your own risk

/*
* #MaterialInstance is a thin wrapper over Exec::#Operation or rather raw #Material
* This class introduces new functionalities and completes the #Material conception.
* It's only through this class that the user will interact with the core material logic,
* configure shader parameters and send the required material resources to the GPU
* The data in the uniform buffer will be transferred with each #MaterialInstance assignment
*/ 
class MaterialInstance
{
public:
	MaterialInstance() : mCoreMaterial(-1) {}

	virtual ~MaterialInstance() = default;

	template <typename Fn>
	void TraverseSampledImageResources(Fn&& fn) const;

	template <typename Fn>
	void TraverseStorageImageResources(Fn&& fn) const;

	template <typename Fn>
	void TraverseStorageBuffers(Fn&& fn) const;

	template <typename Fn>
	void TraverseUniformBuffers(Fn&& fn) const;

	template <typename Fn>
	void TraverseResources(Fn&& fn) const;

	virtual const vkLib::BasicPipeline* GetBasicPipeline() const { return *mCoreMaterial.GetBasicPipeline(); }

	virtual void UpdateDescriptors() const { UpdateMaterialInfos(mCoreMaterial, *mInfo); }

	// you can send only have predefined #GLSL/#HLSL types
	template <typename T>
	std::expected<bool, ShaderParError> SetShaderParameter(const std::string& name, const T& parVal) const;

	AQUA_API void UpdateShaderParBuffer() const;
	AQUA_API void SetOffset(size_t offset) const;

	uint32_t GetOffset() const { return mOffset; }
	MaterialInstanceInfoRef GetInfo() const { return mInfo; }
	MAT_NAMESPACE::Material GetMaterial() const { return mCoreMaterial; }

	std::expected<vkLib::VertexInputDesc, bool> GetVertexBindings() const;
	MAT_NAMESPACE::Platform GetRendererPlatform() const { return mInfo->RendererType; }

	// set 0 and 1 are reserved, so assign them at your own risk
	MAT_NAMESPACE::Resource& operator[](const vkLib::DescriptorLocation& location) const { return mInfo->Resources[location]; }

	consteval static vk::DescriptorType GetSampledImageDescType() { return vk::DescriptorType::eSampledImage; }
	consteval static vk::DescriptorType GetStorageImageDescType() { return vk::DescriptorType::eStorageImage; }
	consteval static vk::DescriptorType GetStorageBufDescType() { return vk::DescriptorType::eStorageBuffer; }
	consteval static vk::DescriptorType GetUniformBufDescType() { return vk::DescriptorType::eUniformBuffer; }

	AQUA_API static void UpdateMaterialInfos(const MAT_NAMESPACE::Material& material, const MaterialInstanceInfo& materialInfo);

protected:
	MaterialInstanceInfoRef mInfo;
	MAT_NAMESPACE::Material mCoreMaterial;
	mutable vkLib::GenericBuffer mShaderParBuffer; // this should probably be in the material instance info
	mutable uint32_t mOffset = 0;
	uint32_t mInstanceID = 0; // Not yet being used

	friend class MaterialBuilder;
};

AQUA_END

template <typename Func>
void AQUA_NAMESPACE::MaterialInstance::TraverseSampledImageResources(Func&& fn) const
{
	for (auto& [loc, resource] : mInfo->Resources)
	{
		if (resource.Type == GetSampledImageDescType())
			fn(loc, resource.ImageView);
	}
}

template <typename Fn>
void AQUA_NAMESPACE::MaterialInstance::TraverseStorageImageResources(Fn&& fn) const
{
	for (auto& [loc, resource] : mInfo->Resources)
	{
		if (resource.Type == GetStorageImageDescType())
			fn(loc, resource.ImageView);
	}
}

template <typename Func>
void AQUA_NAMESPACE::MaterialInstance::TraverseStorageBuffers(Func&& fn) const
{
	for (auto& [loc, resource] : mInfo->Resources)
	{
		if (resource.Type == GetStorageBufDescType())
			fn(loc , resource.Buffer);
	}
}

template <typename Func>
void AQUA_NAMESPACE::MaterialInstance::TraverseUniformBuffers(Func&& fn) const
{
	for (auto& [loc, resource] : mInfo->Resources)
	{
		if (resource.Type == GetUniformBufDescType())
			fn(loc, resource.Buffer);
	}
}

template <typename Fn>
void AQUA_NAMESPACE::MaterialInstance::TraverseResources(Fn&& fn) const
{
	for (auto& [loc, resource] : mInfo->Resources)
	{
		fn(loc, resource);
	}
}

template <typename T>
std::expected<bool, AQUA_NAMESPACE::ShaderParError> AQUA_NAMESPACE::MaterialInstance::
	SetShaderParameter(const std::string& name, const T& parVal) const
{
	if (mInfo->ShaderParameters.find(name) == mInfo->ShaderParameters.end())
		return std::unexpected(ShaderParError::eParameterDoesntExist);

	if (mInfo->ShaderParameters[name].TypeSize != sizeof(parVal))
		return std::unexpected(ShaderParError::eSizeMismatch);

	T* memory = (T*)mShaderParBuffer.MapMemory<uint8_t>(mInfo->ShaderParameters[name].TypeSize,
		mInfo->ShaderParameters[name].Offset + mOffset * mInfo->Stride);

	*memory = parVal;

	mShaderParBuffer.UnmapMemory();

	return true;
}

AQUA_BEGIN

template <typename T>
void SetMaterialPar(const MaterialInstance& instance, const std::string& name, const T& parVal)
{
	auto error = instance.SetShaderParameter(name, parVal)
		.or_else([](ShaderParError val)
	{
		// assert if we fail to set the constant for any reason other than being unable to find it
		_STL_ASSERT(val == ShaderParError::eSizeMismatch, "couldn't set the material parameter");

		// unable to find the constant is fine...
		return std::expected<bool, ShaderParError>(true);
	});
}

AQUA_END
