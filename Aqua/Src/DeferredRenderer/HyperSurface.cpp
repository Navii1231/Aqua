#include "Core/Aqpch.h"
#include "DeferredRenderer/Pipelines/HyperSurface.h"

AQUA_NAMESPACE::HyperSurfacePipeline::HyperSurfacePipeline(HyperSurfaceType type, const glm::vec2& scrSize, 
	vkLib::PShader shader, vkLib::RenderTargetContext targetCtx)
{
	auto config = SetupConfig(type, scrSize);

	config.TargetContext = targetCtx;

	this->SetConfig(config);
	this->SetShader(shader);
}

vkLib::GraphicsPipelineConfig AQUA_NAMESPACE::HyperSurfacePipeline::SetupConfig(HyperSurfaceType type, const glm::vec2& scrSize)
{
	vkLib::GraphicsPipelineConfig config{};

	config.Topology = type == HyperSurfaceType::eLine ?
		vk::PrimitiveTopology::eLineList : vk::PrimitiveTopology::ePointList;
	
	config.VertexInput.Bindings.emplace_back(0, static_cast<uint32_t>(2 * sizeof(glm::vec4)), vk::VertexInputRate::eVertex);
	config.VertexInput.Attributes.emplace_back(0, 0, vk::Format::eR32G32B32A32Sfloat, 0);
	config.VertexInput.Attributes.emplace_back(1, 0, vk::Format::eR32G32B32A32Sfloat, static_cast<uint32_t>(sizeof(glm::vec4)));

	config.Rasterizer.CullMode = vk::CullModeFlagBits::eNone;
	config.Rasterizer.FrontFace = vk::FrontFace::eClockwise;
	config.Rasterizer.LineWidth = 1.0f;
	config.Rasterizer.PolygonMode = type == HyperSurfaceType::eLine ?
		vk::PolygonMode::eLine : vk::PolygonMode::ePoint;

	config.DepthBufferingState.DepthTestEnable = true;
	config.DepthBufferingState.DepthCompareOp = vk::CompareOp::eLess;
	config.DepthBufferingState.DepthWriteEnable = true;
	config.DepthBufferingState.MinDepthBounds = 0.0f;
	config.DepthBufferingState.MaxDepthBounds = 1.0f;

	config.CanvasView = vk::Viewport(0.0f, 0.0f, scrSize.x, scrSize.y, 0.0f, 1.0f);
	config.CanvasScissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D((uint32_t) scrSize.x, (uint32_t) scrSize.y));

	config.DynamicStates.push_back(vk::DynamicState::eDepthTestEnable);
	config.DynamicStates.push_back(vk::DynamicState::eDepthWriteEnable);
	config.DynamicStates.push_back(vk::DynamicState::eViewport);
	config.DynamicStates.push_back(vk::DynamicState::eScissor);

	if(type == HyperSurfaceType::eLine)
		config.DynamicStates.push_back(vk::DynamicState::eLineWidth);

	return config;
}
