#include "Core/Aqpch.h"
#include "DeferredRenderer/Pipelines/SkyboxPipeline.h"

AQUA_BEGIN

SkyboxPipeline::SkyboxPipeline(vkLib::PShader shader, vkLib::Framebuffer framebuffer)
{
	this->SetShader(shader);
	this->SetFramebuffer(framebuffer);

	this->SetConfig(SetupGraphicsConfig(framebuffer.GetParentContext()));
}

void SkyboxPipeline::UpdateCamera(vkLib::Buffer<CameraInfo> camera)
{
	vkLib::UniformBufferWriteInfo uniformInfo{};
	uniformInfo.Buffer = camera.GetNativeHandles().Handle;

	this->UpdateDescriptor({ 0, 0, 0 }, uniformInfo);
}

void SkyboxPipeline::UpdateEnvironmentTexture(vkLib::Image image, vkLib::Core::Ref<vk::Sampler> sampler)
{
	if (!image || !sampler)
		return;

	vkLib::SampledImageWriteInfo sampledImage{};

	sampledImage.ImageLayout = vk::ImageLayout::eGeneral;
	sampledImage.ImageView = image.GetIdentityImageView().GetNativeHandle();
	sampledImage.Sampler = *sampler;

	if (!GetShader().IsEmpty(0, 1))
		this->UpdateDescriptor({ 0, 1, 0 }, sampledImage);
}

vkLib::GraphicsPipelineConfig AQUA_NAMESPACE::SkyboxPipeline::SetupGraphicsConfig(vkLib::RenderTargetContext ctx)
{
	auto resolution = GetFramebuffer().GetResolution();

	vkLib::GraphicsPipelineConfig config{};
	config.CanvasScissor.offset = vk::Offset2D(0, 0);
	config.CanvasScissor.extent = vk::Extent2D(resolution.x, resolution.y);
	config.CanvasView = vk::Viewport(0.0f, 0.0f, (float)resolution.x, (float)resolution.y, 0.0f, 1.0f);

	config.IndicesType = vk::IndexType::eUint32;
	config.SubpassIndex = 0;
	config.Topology = vk::PrimitiveTopology::eTriangleList;

	config.TargetContext = ctx;

	config.DepthBufferingState.DepthBoundsTestEnable = false;
	config.DepthBufferingState.DepthCompareOp = vk::CompareOp::eLessOrEqual;
	config.DepthBufferingState.DepthTestEnable = true;
	config.DepthBufferingState.DepthWriteEnable = false;
	config.DepthBufferingState.MaxDepthBounds = 1.0f;
	config.DepthBufferingState.MinDepthBounds = 0.0f;
	config.DepthBufferingState.StencilBackOp = vk::StencilOpState{};
	config.DepthBufferingState.StencilFrontOp = vk::StencilOpState{};
	config.DepthBufferingState.StencilTestEnable = false;

	config.DynamicStates.emplace_back(vk::DynamicState::eScissor);
	config.DynamicStates.emplace_back(vk::DynamicState::eViewport);
	config.DynamicStates.emplace_back(vk::DynamicState::eLineWidth);
	config.DynamicStates.emplace_back(vk::DynamicState::eDepthCompareOp);

	return config;
}

AQUA_END

