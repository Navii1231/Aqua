#include "Core/vkpch.h"
#include "Device/Context.h"
#include "Pipeline/PipelineBuilder.h"

VK_BEGIN

BasicPipeline Clone(Context ctx, const BasicPipeline& pipeline)
{
	auto bindPoint = pipeline.GetPipelineBindPoint();
	auto pipelineBuilder = ctx.MakePipelineBuilder();

	switch (bindPoint)
	{
	case vk::PipelineBindPoint::eGraphics:
		return pipelineBuilder.BuildGraphicsPipeline<GraphicsPipeline>(pipeline.GetShader());
	case vk::PipelineBindPoint::eCompute:
		return pipelineBuilder.BuildComputePipeline<ComputePipeline>(pipeline.GetShader());
	case vk::PipelineBindPoint::eRayTracingKHR:
		_STL_VERIFY(false, "at cloning - pipeline unsupported yet");
		return {};
	default:
		_STL_VERIFY(false, "cloning bad pipeline");
		return {};
	}
}

ComputePipeline Clone(Context ctx, const ComputePipeline& pipeline)
{
	return ctx.MakePipelineBuilder().BuildComputePipeline<ComputePipeline>(pipeline);
}

GraphicsPipeline Clone(Context ctx, const GraphicsPipeline& pipeline)
{
	return ctx.MakePipelineBuilder().BuildGraphicsPipeline<GraphicsPipeline>(pipeline);
}

static std::unordered_map<vk::DynamicState, GraphicsDynamicStateFn> sGraphicsFn =
{
	// Viewport state
	{ vk::DynamicState::eViewport, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setViewport(0, config.CanvasView);
	}},

	{ vk::DynamicState::eScissor, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setScissor(0, config.CanvasScissor);
	}},

	// Rasterizer state
	{ vk::DynamicState::eCullMode, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setCullMode(config.Rasterizer.CullMode);
	}},

	{ vk::DynamicState::eFrontFace, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setFrontFace(config.Rasterizer.FrontFace);
	}},

	{ vk::DynamicState::eLineWidth, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setLineWidth(config.Rasterizer.LineWidth);
	}},

	//{ vk::DynamicState::eDepthBias, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	//{
	//	// hasn't yet been implemented
	//	cmd.setDepthBias(0.0f, 0.0f, 0.0f);
	//}},

	//{ vk::DynamicState::eBlendConstants, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	//{
	//	const float constants[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	//	cmd.setBlendConstants(constants);
	//}},

	//{ vk::DynamicState::ePolygonModeEXT, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	//{
	//	cmd.setPolygonModeEXT(config.Rasterizer.PolygonMode); // requires VK_EXT_extended_dynamic_state2
	//}},

	// Depth state
	{ vk::DynamicState::eDepthTestEnable, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setDepthTestEnable(config.DepthBufferingState.DepthTestEnable); // requires VK_EXT_extended_dynamic_state
	}},

	{ vk::DynamicState::eDepthWriteEnable, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setDepthWriteEnable(config.DepthBufferingState.DepthWriteEnable); // requires VK_EXT_extended_dynamic_state
	}},

	{ vk::DynamicState::eDepthCompareOp, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setDepthCompareOp(config.DepthBufferingState.DepthCompareOp); // requires VK_EXT_extended_dynamic_state
	}},

	{ vk::DynamicState::eDepthBoundsTestEnable, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setDepthBoundsTestEnable(config.DepthBufferingState.DepthBoundsTestEnable); // requires VK_EXT_extended_dynamic_state
	}},

	{ vk::DynamicState::eDepthBounds, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setDepthBounds(config.DepthBufferingState.MinDepthBounds, config.DepthBufferingState.MaxDepthBounds);
	}},

	// Stencil state
	{ vk::DynamicState::eStencilTestEnable, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setStencilTestEnable(config.DepthBufferingState.StencilTestEnable); // requires VK_EXT_extended_dynamic_state
	}},

	{ vk::DynamicState::eStencilOp, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setStencilOp(
			vk::StencilFaceFlagBits::eFrontAndBack,
			config.DepthBufferingState.StencilFrontOp.failOp,
			config.DepthBufferingState.StencilFrontOp.passOp,
			config.DepthBufferingState.StencilFrontOp.depthFailOp,
			config.DepthBufferingState.StencilFrontOp.compareOp
		); // applies to both front/back unless using EXT2 variant
	}},

	{ vk::DynamicState::eStencilCompareMask, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setStencilCompareMask(vk::StencilFaceFlagBits::eFront, config.DepthBufferingState.StencilFrontOp.compareMask);
		cmd.setStencilCompareMask(vk::StencilFaceFlagBits::eBack, config.DepthBufferingState.StencilBackOp.compareMask);
	}},

	{ vk::DynamicState::eStencilWriteMask, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setStencilWriteMask(vk::StencilFaceFlagBits::eFront, config.DepthBufferingState.StencilFrontOp.writeMask);
		cmd.setStencilWriteMask(vk::StencilFaceFlagBits::eBack, config.DepthBufferingState.StencilBackOp.writeMask);
	}},

	{ vk::DynamicState::eStencilReference, [](vk::CommandBuffer cmd, const VK_NAMESPACE::GraphicsPipelineConfig& config)
	{
		cmd.setStencilReference(vk::StencilFaceFlagBits::eFront, config.DepthBufferingState.StencilFrontOp.reference);
		cmd.setStencilReference(vk::StencilFaceFlagBits::eBack, config.DepthBufferingState.StencilBackOp.reference);
	}},
};

VK_END

VK_NAMESPACE::GraphicsDynamicStateFn VK_NAMESPACE::GetGraphicsDynamicFunction(vk::DynamicState state)
{
	return sGraphicsFn.at(state);
}
