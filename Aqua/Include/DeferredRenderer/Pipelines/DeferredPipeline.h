#pragma once
#include "PipelineConfig.h"
#include "../Renderable/Renderable.h"
#include "../Renderable/CopyIndices.h"
#include "../Renderable/RenderableBuilder.h"

AQUA_BEGIN

/* --> For most optimal rendering, we could
* Give each deferred renderable a vertex and index buffer instance and a compute pipeline
* When submitting a renderable to graphics pipeline, all vertices and indices can be
* copied entirely through the compute pipeline, therefore avoiding any CPU work
*/

/*
* --> There are three ways we can implement this thing
* --> We're currently utilizing the third one in this implementation
* First method: to copy the vertex attributes at each frame
*	-- more efficient for frequently removing and adding stuff
* Second method: to save the whole scene at once and then move from CPU to GPU only when the scene changes
*	-- might become inefficient when large meshes are added or removed frequently
* Third method: use compute shaders or GPU command buffers to copy data from one buffer to another
*	-- seems like a nice middle ground
*/

using DeferredRenderable = Renderable;

// TODO: volatile to future changes
class DeferredPipeline : public vkLib::GraphicsPipeline
{
public:
	DeferredPipeline() = default;
	// using default tags
	AQUA_API DeferredPipeline(const vkLib::PShader shader, vkLib::Framebuffer buffer, const VertexBindingMap& bindings);

	AQUA_API void UpdateModels(Mat4Buf models);
	AQUA_API void UpdateCamera(CameraBuf camera);

	virtual void Cleanup() {}
private:
	// Helper functions...

	void SetupPipelineConfig(vkLib::GraphicsPipelineConfig& config, const glm::uvec2& scrSize, const VertexBindingMap& bindings);

	void SetupPipeline(vkLib::PShader shader, vkLib::Framebuffer framebuffer, const VertexBindingMap& bindings);

	void MakeHollow();
};

inline DeferredPipeline Clone(vkLib::Context ctx, const DeferredPipeline& pipeline)
{
	return ctx.MakePipelineBuilder().BuildGraphicsPipeline<DeferredPipeline>(pipeline);
}


AQUA_END

