#pragma once
#include "PipelineConfig.h"

AQUA_BEGIN

struct SkyboxVertex
{
	glm::vec3 Vertex;
};

class SkyboxPipeline : public vkLib::GraphicsPipeline
{
public:
	SkyboxPipeline() = default;

	AQUA_API SkyboxPipeline(vkLib::PShader shader, vkLib::Framebuffer framebuffer);

	virtual ~SkyboxPipeline() = default;

	AQUA_API void UpdateCamera(vkLib::Buffer<CameraInfo> camera);
	AQUA_API void UpdateEnvironmentTexture(vkLib::Image image, vkLib::Core::Ref<vk::Sampler> sampler);

private:
	virtual size_t GetIndexCount() const override { return 6; }

	vkLib::GraphicsPipelineConfig SetupGraphicsConfig(vkLib::RenderTargetContext ctx);
};

inline SkyboxPipeline Clone(vkLib::Context ctx, const SkyboxPipeline& pipeline)
{
	return ctx.MakePipelineBuilder().BuildGraphicsPipeline<SkyboxPipeline>(pipeline);
}

AQUA_END
