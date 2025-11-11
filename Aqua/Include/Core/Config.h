#pragma once
#include "AqCore.h"

AQUA_BEGIN

template <typename BasePipeline, typename T>
void PushConst(const BasePipeline& pipeline, const std::string& name, const T& constant)
{
	auto error = pipeline.SetShaderConstant(name, constant)
		.or_else([](vkLib::ShaderConstantError val)
			{
				// assert if we fail to push the constant for any reason other than being unable to find it
				_STL_ASSERT(val.Type == vkLib::ShaderConstantErrorType::eFailedToFindPushConstant, val.Info.c_str());

				// unable to find the constant is fine...
				return std::expected<bool, vkLib::ShaderConstantError>(true);
			});
}

template <typename BasePipeline>
void PushConst(const BasePipeline& pipeline, const std::string& name, size_t size, const void* ptr)
{
	auto error = pipeline.SetShaderConstant(name, size, ptr)
		.or_else([](vkLib::ShaderConstantError val)
			{
				// assert if we fail to push the constant for any reason other than being unable to find it
				_STL_ASSERT(val.Type == vkLib::ShaderConstantErrorType::eFailedToFindPushConstant, val.Info.c_str());

				// unable to find the constant is fine...
				return std::expected<bool, vkLib::ShaderConstantError>(true);
			});
}

// re-creating the resource under new context
template <typename _Rsc>
_Rsc Clone(VK_NAMESPACE::Context ctx, const _Rsc& rsc)
{
	// by default, we call the VulkanLibrary Clone function
	return VK_NAMESPACE::Clone(ctx, rsc);
}

AQUA_END
