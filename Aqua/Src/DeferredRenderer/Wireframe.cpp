#include "Core/Aqpch.h"
#include "DeferredRenderer/Pipelines/Wireframe.h"

AQUA_NAMESPACE::WireframePipeline::WireframePipeline(vkLib::PShader shader, 
	const glm::uvec2& scrSize, vkLib::RenderTargetContext ctx)
{
	auto config = SetupConfig(scrSize);

	config.TargetContext = ctx;

	this->SetShader(shader);
	this->SetConfig(config);
}

vkLib::GraphicsPipelineConfig AQUA_NAMESPACE::WireframePipeline::SetupConfig(const glm::uvec2& scrSize) const
{
	vkLib::GraphicsPipelineConfig config{};

	config.CanvasScissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(scrSize.x, scrSize.y));
	config.CanvasView = vk::Viewport(0.0f, 0.0f, (float)scrSize.x, (float)scrSize.y, 0.0f, 1.0f);

	config.Topology = vk::PrimitiveTopology::eLineList;

	config.VertexInput.Bindings.emplace_back(0, static_cast<uint32_t>(sizeof(glm::vec3)), vk::VertexInputRate::eVertex);
	config.VertexInput.Bindings.emplace_back(1, static_cast<uint32_t>(sizeof(glm::vec3)), vk::VertexInputRate::eVertex);

	config.VertexInput.Attributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, 0);
	config.VertexInput.Attributes.emplace_back(1, 1, vk::Format::eR32G32B32Sfloat, 0);

	config.DepthBufferingState.DepthTestEnable = true;
	config.DepthBufferingState.DepthCompareOp = vk::CompareOp::eLess;
	config.DepthBufferingState.DepthWriteEnable = true;
	config.DepthBufferingState.DepthBoundsTestEnable = false;

	config.Rasterizer.CullMode = vk::CullModeFlagBits::eNone;
	config.Rasterizer.FrontFace = vk::FrontFace::eClockwise;
	config.Rasterizer.LineWidth = 1.5f;
	config.Rasterizer.PolygonMode = vk::PolygonMode::eLine;

	config.DynamicStates.push_back(vk::DynamicState::eDepthCompareOp);
	config.DynamicStates.push_back(vk::DynamicState::eDepthTestEnable);
	config.DynamicStates.push_back(vk::DynamicState::eDepthWriteEnable);
	config.DynamicStates.push_back(vk::DynamicState::eLineWidth);
	config.DynamicStates.push_back(vk::DynamicState::eScissor);
	config.DynamicStates.push_back(vk::DynamicState::eViewport);

	return config;
}
