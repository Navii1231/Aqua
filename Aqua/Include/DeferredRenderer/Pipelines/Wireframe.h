#pragma once
#include "PipelineConfig.h"

AQUA_BEGIN

class WireframePipeline : public vkLib::GraphicsPipeline
{
public:
	WireframePipeline() = default;
	AQUA_API WireframePipeline(vkLib::PShader shader, const glm::uvec2& scrSize, vkLib::RenderTargetContext ctx);

	~WireframePipeline() = default;

private:
	vkLib::GraphicsPipelineConfig SetupConfig(const glm::uvec2& scrSize) const;
};

inline WireframePipeline Clone(vkLib::Context ctx, const WireframePipeline& pipeline)
{
	return ctx.MakePipelineBuilder().BuildGraphicsPipeline<WireframePipeline>(pipeline);
}

AQUA_END
