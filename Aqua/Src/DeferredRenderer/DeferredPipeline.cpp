#include "Core/Aqpch.h"
#include "DeferredRenderer/Pipelines/DeferredPipeline.h"
#include "Utils/CompilerErrorChecker.h"

AQUA_NAMESPACE::DeferredPipeline::DeferredPipeline(const vkLib::PShader shader, vkLib::Framebuffer buffer, const VertexBindingMap& bindings)
{
	SetupPipeline(shader, buffer, bindings);
	SetFramebuffer(buffer);
}

void AQUA_NAMESPACE::DeferredPipeline::UpdateModels(Mat4Buf models)
{
	vkLib::StorageBufferWriteInfo storageInfo{ models.GetNativeHandles().Handle };
	this->UpdateDescriptor({ 0, 1, 0 }, storageInfo);
}

void AQUA_NAMESPACE::DeferredPipeline::UpdateCamera(CameraBuf camera)
{
	vkLib::UniformBufferWriteInfo uniformInfo{};
	uniformInfo.Buffer = camera.GetNativeHandles().Handle;

	this->UpdateDescriptor({ 0, 0, 0 }, uniformInfo);
}

void AQUA_NAMESPACE::DeferredPipeline::SetupPipelineConfig(vkLib::GraphicsPipelineConfig& config, const glm::uvec2& scrSize, const VertexBindingMap& bindings)
{
	config.CanvasScissor.offset = vk::Offset2D(0, 0);
	config.CanvasScissor.extent = vk::Extent2D(scrSize.x, scrSize.y);
	config.CanvasView = vk::Viewport(0.0f, 0.0f, (float)scrSize.x, (float)scrSize.y, 0.0f, 1.0f);

	config.IndicesType = vk::IndexType::eUint32;

	config.VertexInput = VertexFactory::GenerateVertexInputStreamInfo(bindings);

	config.SubpassIndex = 0;

	config.DepthBufferingState.DepthBoundsTestEnable = false;
	config.DepthBufferingState.DepthCompareOp = vk::CompareOp::eLess;
	config.DepthBufferingState.DepthTestEnable = true;
	config.DepthBufferingState.DepthWriteEnable = true;
	config.DepthBufferingState.MaxDepthBounds = 1.0f;
	config.DepthBufferingState.MinDepthBounds = 0.0f;
	config.DepthBufferingState.StencilTestEnable = false;

	config.Rasterizer.CullMode = vk::CullModeFlagBits::eNone;
	config.Rasterizer.FrontFace = vk::FrontFace::eCounterClockwise;
	config.Rasterizer.LineWidth = 0.01f;
	config.Rasterizer.PolygonMode = vk::PolygonMode::eFill;

	config.DynamicStates.emplace_back(vk::DynamicState::eScissor);
	config.DynamicStates.emplace_back(vk::DynamicState::eViewport);
	config.DynamicStates.emplace_back(vk::DynamicState::eLineWidth);
	config.DynamicStates.emplace_back(vk::DynamicState::eDepthCompareOp);
}

void AQUA_NAMESPACE::DeferredPipeline::SetupPipeline(vkLib::PShader shader, vkLib::Framebuffer framebuffer, const VertexBindingMap& bindings)
{
	this->SetShader(shader);

	vkLib::GraphicsPipelineConfig config;

	// Generating pipeline config
	SetupPipelineConfig(config, framebuffer.GetResolution(), bindings);

	config.TargetContext = framebuffer.GetParentContext();

	this->SetConfig(config);
}

