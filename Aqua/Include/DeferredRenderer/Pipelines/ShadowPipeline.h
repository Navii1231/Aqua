#pragma once
#include "PipelineConfig.h"

AQUA_BEGIN

class ShadowPipeline : public vkLib::GraphicsPipeline
{
public:
	ShadowPipeline() = default;
	AQUA_API ShadowPipeline(vkLib::PShader shader, vkLib::Framebuffer depthBuffer, const VertexBindingMap& vertexBindings);

	AQUA_API void UpdateCamera(CameraBuf camera);
	AQUA_API void UpdateModels(Mat4Buf models);

	virtual ~ShadowPipeline() = default;

private:
	void SetupConfig(vkLib::GraphicsPipelineConfig& config, const glm::uvec2& scrSize, const VertexBindingMap& bindings);
};

inline ShadowPipeline Clone(vkLib::Context ctx, const ShadowPipeline& pipeline)
{
	return ctx.MakePipelineBuilder().BuildGraphicsPipeline<ShadowPipeline>(pipeline);
}

AQUA_END
