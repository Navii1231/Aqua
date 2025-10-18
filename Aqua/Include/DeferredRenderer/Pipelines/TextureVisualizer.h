#pragma once
#include "PipelineConfig.h"

AQUA_BEGIN

class TextureVisualizer : public vkLib::GraphicsPipeline
{
public:
	TextureVisualizer() = default;
	inline TextureVisualizer(const vkLib::PShader& shader, vkLib::Framebuffer framebuffer);

	inline void UpdateTexture(vkLib::ImageView texture, vkLib::Core::Ref<vk::Sampler> sampler);

private:
	inline vkLib::GraphicsPipelineConfig SetupConfig(const glm::uvec2& scrSize);
};

inline TextureVisualizer Clone(vkLib::Context ctx, const TextureVisualizer& pipeline)
{
	return ctx.MakePipelineBuilder().BuildGraphicsPipeline<TextureVisualizer>(pipeline);
}

TextureVisualizer::TextureVisualizer(const vkLib::PShader& shader, vkLib::Framebuffer framebuffer)
{
	vkLib::GraphicsPipelineConfig config = SetupConfig(framebuffer.GetResolution());

	config.TargetContext = framebuffer.GetParentContext();

	this->SetFramebuffer(framebuffer);
	this->SetShader(shader);
	this->SetConfig(config);
}

vkLib::GraphicsPipelineConfig TextureVisualizer::SetupConfig(const glm::uvec2& scrSize)
{
	vkLib::GraphicsPipelineConfig config{};

	config.CanvasScissor = vk::Rect2D({ 0, 0 }, { scrSize.x, scrSize.y });
	config.CanvasView = vk::Viewport(0.0f, 0.0f, (float)scrSize.x, (float)scrSize.y, 0.0f, 1.0f);

	config.IndicesType = vk::IndexType::eUint32;

	config.SubpassIndex = 0;

	config.DepthBufferingState.DepthBoundsTestEnable = false;
	config.DepthBufferingState.DepthCompareOp = vk::CompareOp::eLess;
	config.DepthBufferingState.DepthTestEnable = true;
	config.DepthBufferingState.DepthWriteEnable = false;
	config.DepthBufferingState.MaxDepthBounds = 1.0f;
	config.DepthBufferingState.MinDepthBounds = 0.0f;
	config.DepthBufferingState.StencilTestEnable = false;

	config.Rasterizer.CullMode = vk::CullModeFlagBits::eNone;
	config.Rasterizer.FrontFace = vk::FrontFace::eCounterClockwise;
	config.Rasterizer.LineWidth = 0.01f;
	config.Rasterizer.PolygonMode = vk::PolygonMode::eFill;

	return config;
}

void TextureVisualizer::UpdateTexture(vkLib::ImageView texture, vkLib::Core::Ref<vk::Sampler> sampler)
{
	if (!texture || !sampler)
		return;

	vkLib::SampledImageWriteInfo samplerInfo{};
	samplerInfo.ImageLayout = vk::ImageLayout::eGeneral;
	samplerInfo.ImageView = texture.GetNativeHandle();
	samplerInfo.Sampler = *sampler;

	this->UpdateDescriptor({ 0, 0, 0 }, samplerInfo);
}

AQUA_END
