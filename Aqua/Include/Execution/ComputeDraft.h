#pragma once
#include "Draft.h"
#include "ComputeNode.h"
#include "Parser.h"

AQUA_BEGIN
EXEC_BEGIN

struct ComputeNodeInfo
{
	std::string MainKernel{};
	std::vector<std::string> SecondaryKernels{};

	ComputeNodeInfo() = default;
	~ComputeNodeInfo() = default;

	ComputeNodeInfo(const ComputeNodeInfo&) = default;
	ComputeNodeInfo(const std::string& mainKernel) : MainKernel(mainKernel) {}
};

// mimics CUDA kernels
// only accepts modified GLSL shaders
// generates a network of compute pipelines with shared resources
class ComputeDraft : public Draft<ComputeNodeInfo, vk::PipelineStageFlags>
{
public:
	ComputeDraft() = default;

	ComputeDraft(vkLib::Context ctx)
		: mCtx(ctx) { }

	void SetCtx(vkLib::Context ctx) { mCtx = ctx; }

	AQUA_API std::expected<Graph, GraphError> Construct(const std::vector<NodeID>& probes, int threadCount = 1) const;

private:
	vkLib::Context mCtx;          

private:
	vkLib::PShader ConstructShader(const KernelExtractions& exts) const;
	std::expected<bool, GraphError> InsertResources(ComputeNode& node, const KernelExtractions& exts) const;
};

EXEC_END
AQUA_END
