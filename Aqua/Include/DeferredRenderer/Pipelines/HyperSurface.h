#pragma once
#include "../../Core/AqCore.h"

AQUA_BEGIN

enum class HyperSurfaceType
{
	ePoint        = 1,
	eLine         = 2,
};

class HyperSurfacePipeline : public vkLib::GraphicsPipeline
{
public:
	HyperSurfacePipeline() = default;
	~HyperSurfacePipeline() = default;

	AQUA_API HyperSurfacePipeline(HyperSurfaceType surfaceType, const glm::vec2& scrSize,
		vkLib::PShader shader, vkLib::RenderTargetContext targetCtx);

private:
	vkLib::GraphicsPipelineConfig SetupConfig(HyperSurfaceType type, const glm::vec2& scrSize);
};

inline HyperSurfacePipeline Clone(vkLib::Context ctx, const HyperSurfacePipeline& pipeline)
{
	return ctx.MakePipelineBuilder().BuildGraphicsPipeline<HyperSurfacePipeline>(pipeline);
}

AQUA_END
