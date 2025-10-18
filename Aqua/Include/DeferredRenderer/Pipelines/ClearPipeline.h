#pragma once
#include "PipelineConfig.h"

AQUA_BEGIN

class ClearPipeline : public vkLib::ComputePipeline
{
public:
	ClearPipeline() = default;
	~ClearPipeline() = default;

	AQUA_API ClearPipeline(const std::string& format, const glm::uvec2& workGroupSize);

	AQUA_API void operator()(vk::CommandBuffer buffer, vkLib::ImageView view, const glm::vec4& image) const;
};

inline ClearPipeline Clone(vkLib::Context ctx, const ClearPipeline& clearPipeline)
{
	return ctx.MakePipelineBuilder().BuildComputePipeline<ClearPipeline>(clearPipeline);
}

AQUA_END
