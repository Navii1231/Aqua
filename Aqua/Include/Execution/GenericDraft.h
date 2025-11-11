#pragma once
#include "GraphConfig.h"
#include "CBScope.h"
#include "Graph.h"
#include "ParserConfig.h"
#include "Draft.h"
#include "GenericNode.h"

AQUA_BEGIN
EXEC_BEGIN

class GenericDraft : public Draft<GenericNode, vk::PipelineStageFlags>
{
public:
	GenericDraft() = default;
	GenericDraft(vkLib::Context ctx) : mCtx(ctx) {}

	~GenericDraft() = default;

	void SetCtx(vkLib::Context ctx) { mCtx = ctx; }

	AQUA_API void Clear();

	void ClearOperations() { ClearNodes(); }

	// operations
	void SubmitOperation(NodeID node) { this->operator[](node) = GenericNode(node, OpType::eNone); }

	template <typename _Pipeline>
	void SubmitPipeline(NodeID nodeId, const _Pipeline& pipeline);

	// Redundant operations that do no contribute to the final outcome are excluded
	// convergent on the probe node
	AQUA_API std::expected<Graph, GraphError> Construct(const std::vector<NodeID>& pathEnds) const;

	const std::map<NodeID, GenericNode>& GetOps() const { return GetNodes(); }
	NodeID GetOpCount() const { return static_cast<NodeID>(GetNodeCount()); }

private:

	vkLib::Context mCtx;
};

template <typename _Pipeline>
void AQUA_NAMESPACE::EXEC_NAMESPACE::GenericDraft::SubmitPipeline(NodeID nodeId, const _Pipeline& pipeline)
{
	GenericNode& opRef = this->operator[](nodeId);

	opRef.NodeId = nodeId;

	switch (pipeline.GetPipelineBindPoint())
	{
	case vk::PipelineBindPoint::eGraphics:
		opRef.Type = OpType::eGraphics;
		opRef.GFX = std::reinterpret_pointer_cast<vkLib::GraphicsPipeline>(MakeRef<_Pipeline>(pipeline));
		break;
	case vk::PipelineBindPoint::eCompute:
		opRef.Type = OpType::eCompute;
		opRef.Cmp = std::reinterpret_pointer_cast<vkLib::ComputePipeline>(MakeRef<_Pipeline>(pipeline));
		break;
	case vk::PipelineBindPoint::eRayTracingKHR:
		opRef.Type = OpType::eRayTracing;
		_STL_ASSERT(false, "Ray tracing pipeline is yet to implement in the vkLib");
		break;
	default:
		return;
	}
}

void SerializeExecutionWavefronts(GenericDraft& builder, const std::vector<Wavefront>& layers);

EXEC_END
AQUA_END
