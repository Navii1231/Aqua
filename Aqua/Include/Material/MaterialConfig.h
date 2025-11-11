#pragma once
#include "../Core/AqCore.h"
#include "../Core/SharedRef.h"
#include "../Execution/Graph.h"

AQUA_BEGIN
MAT_BEGIN

enum class Platform
{
	ePathTracer                  = 1,
	eLightingRaster              = 2,
	eLightingCompute             = 3,
};

enum class MaterialPostprocessState
{
	eSuccess                     = 1,
	eInvalidCustomParameter      = 2,
	eEmptyString                 = 3,
	eInvalidImportDirective      = 4,
	eShaderNotFound              = 5,
};

struct MaterialGraphPostprocessError
{
	std::string Info;
	MaterialPostprocessState State = MaterialPostprocessState::eSuccess;
};

struct ShaderTemplate
{
	std::string_view FrontEnd;
	std::string_view BackEnd;

	Platform Platform = Platform::eLightingRaster;
};

struct Resource
{
	vkLib::Core::BufferResource Buffer;
	vkLib::ImageView ImageView;
	vkLib::Core::Ref<vk::Sampler> Sampler;

	vk::DescriptorType Type = vk::DescriptorType::eSampledImage;

	Resource() = default;

	void SetStorageBuffer(const vkLib::Core::BufferResource& buffer)
	{
		Buffer = buffer;
		Type = vk::DescriptorType::eStorageBuffer;
	}

	void SetUniformBuffer(const vkLib::Core::BufferResource& buffer)
	{
		Buffer = buffer;
		Type = vk::DescriptorType::eUniformBuffer;
	}

	void SetSampledImage(vkLib::ImageView view, vkLib::Core::Ref<vk::Sampler> sampler)
	{
		ImageView = view;
		Sampler = sampler;
		Type = vk::DescriptorType::eSampledImage;
	}

	void SetStorageImage(vkLib::ImageView view)
	{
		ImageView = view;
		Type = vk::DescriptorType::eStorageImage;
	}
};

/*
* A #Material is essentially a #GenericNode instance, a part of the shader that can run on the GPU
* It will be hidden in the #MaterialInstance class in a way that the client can't
* modify it. Same material can be shared across different set of 
* #MaterialInstance's with different parameter values, thus encouraging 
* pipeline caching and reducing the need to create a pipeline multiple times.
*/ 
using Material = ::AQUA_NAMESPACE::EXEC_NAMESPACE::GenericNode;

/*
* A #shader_parameter represent custom values set by the user that can affect
* the shading in real time. In a handwritten shader, the #parser will see them in the form of
* #basic_type.#name, where #basic_type is any basic shader type such as #int, 
* #uint, #float, #vec3, #mat2 so on and so forth, and [#name] is the parameter name while [#idx] 
* is the parameter index, it can starts from zero can extend to as much as the #GLSL/#HLSL struct 
* definitions can hold. When the #parser reads the whole text, a corresponding struct 
* #ShaderParameterSet will be generated 
* added for the shader
* 
* The definition produced in the shader side may look like
* 
*	struct #CPU_CustomParameterSet (or #CPU_ShaderParameterSet)
*	{
*		@(vec3.BaseColor).Type @(vec3.BaseColor).Name;
*		@(float.Roughness).Type @(float.Roughness).Name;
*		.
*		.
*		@(float.RefractIdx).Type @(float.RefractIdx).Name;
*	};
* 
*	layout(std140, set = #SHADER_PARS_SET_IDX, binding = #SHADER_PARS_BINDING_IDX) readonly buffer #ShaderParameterSets
*	{
*		#CPU_ShaderParameterSet sCPU_CustomParameters[];
*	}; // #SHADER_PARS_SET_IDX and #SHADER_PARS_BINDING_IDX are reserved #set/#bindings and configured through the macros
* 
* All triangles, quads or any kind of draw primitives will carry two indices somewhere in their vertex buffers
* Where these indices can be found is left to the implementation of the vertex buffer streaming to decide
*	-> #First index specifies with which material the triangle is going to be shaded
*	-> #Second one specifies the offset into the ShaderParameterSets buffer. The primitives using the
*	   use same material may still require a set with different custom parameters, thus we need a way to 
*	   a use variety of parameter sets for the same material pipeline. This is where the #MaterialInstance class
*	   will be helpful to specify the unique index
* 
* if #parOffset is second index specifying the offset into material properties, then the 
* transformation could occur like this:
*	
*		@(vec3.BaseColor) --> sCPU_CustomParameters[parOffset].BaseColor
* 
* On the compilation front, the #parser will replace each instance of [#basic_type]._[#idx] with the 
* corresponding entry name taken from the #table/#struct above
* 
* Finally, a vkLib::GenericBuffer instance will be generated in the CPU side that will be held
* by the #MaterialInstance class.
* 
*	vkLib::GenericBuffer MaterialInstance::ShaderParameterSet;
*	template<typename T>
*	void MaterialInstance::SetShaderParameter(uint32_t parIdx, T&& parVal);
*/
struct ShaderParameter
{
	std::string Type;
	std::string Name;
	uint32_t Offset = 0;
	uint32_t TypeSize = 0;
	uint32_t Alignment = 0;
};

struct DescriptorHasher
{
	size_t operator()(const ::VK_NAMESPACE::DescriptorLocation& location) const
	{
		return ::VK_NAMESPACE::CreateHash(location);
	}
};

using ShaderParameterSet = std::unordered_map<std::string, ShaderParameter>;
// #import name --> #module text
using ShaderImportMap = std::unordered_map<std::string, std::string>;
using ResourceMap = std::unordered_map<vkLib::DescriptorLocation, Resource, DescriptorHasher>;

MAT_END
AQUA_END
