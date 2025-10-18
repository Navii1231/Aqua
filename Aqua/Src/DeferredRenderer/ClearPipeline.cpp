#include "Core/Aqpch.h"
#include "DeferredRenderer/Pipelines/ClearPipeline.h"

AQUA_BEGIN

static std::string sClearShaderCode =
R"(#version 440 core

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;
layout(set = 0, binding = 0, IMAGE_FORMAT) writeonly uniform image2D uImage;

layout(push_constant) uniform ShaderConstants
{
	vec4 pClearValue;
};

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(uImage);
	if (coord.x >= size.x || coord.y >= size.y) return;

	imageStore(uImage, coord, pClearValue);
})";

AQUA_END

AQUA_NAMESPACE::ClearPipeline::ClearPipeline(const std::string& format, const glm::uvec2& workGroupSize)
{
	vkLib::PShader shader;

	shader.SetShader("eCompute", sClearShaderCode);

	shader.AddMacro("IMAGE_FORMAT", format);
	shader.AddMacro("LOCAL_SIZE_X", std::to_string(workGroupSize.x));
	shader.AddMacro("LOCAL_SIZE_Y", std::to_string(workGroupSize.y));

	auto errors = shader.CompileShaders();

	this->SetShader(shader);
}

void AQUA_NAMESPACE::ClearPipeline::operator()(vk::CommandBuffer buffer, vkLib::ImageView view, const glm::vec4& clearValue) const
{
	vkLib::StorageImageWriteInfo storageImageInfo{};
	storageImageInfo.ImageLayout = vk::ImageLayout::eGeneral;
	storageImageInfo.ImageView = view.GetNativeHandle();

	this->UpdateDescriptor({ 0, 0, 0 }, storageImageInfo);

	view->BeginCommands(buffer);
	view->RecordTransitionLayout(storageImageInfo.ImageLayout);

	glm::uvec3 workCount = { view.GetSize().x / this->GetWorkGroupSize().x + 1,
	view.GetSize().y / this->GetWorkGroupSize().y + 1, 1};

	this->Begin(buffer);

	this->Activate();

	this->SetShaderConstant("eCompute.ShaderConstants.Index_0", clearValue);
	this->Dispatch(workCount);

	this->End();

	view->EndCommands();
}
