#include "Core/Aqpch.h"
#include "Execution/GenericDraft.h"
#include "Execution/Graph.h"
#include "Execution/GenericNode.h"

// not needed here, compute draft stuff
#include "Execution/Parser.h"
#include "Execution/CodeGenerator.h"

#include "Utils/CompilerErrorChecker.h"


void AQUA_NAMESPACE::EXEC_NAMESPACE::GenericDraft::Clear()
{
	ClearDependencies();
	ClearOperations();
}

std::expected<AQUA_NAMESPACE::EXEC_NAMESPACE::Graph, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError>
	AQUA_NAMESPACE::EXEC_NAMESPACE::GenericDraft::Construct(
	const std::vector<NodeID>& pathEnds) const
{
	using NodeInfo = typename MyDraftType::NodeInfo<NodeRef>;

	return _ConstructEx<NodeRef>(pathEnds, true, [this](NodeID nodeId, const GenericNode& node)->std::expected<NodeRef, GraphError>
		{
			return MakeRef(node);
		}, [this](const NodeInfo& from, const NodeInfo& to, vk::PipelineStageFlags stageFlags)->std::expected<bool, GraphError>
			{
				Dependency dependency{};
				dependency.SetIncomingOP(from.ID);
				dependency.SetOutgoingOP(to.ID);
				dependency.SetSignal(mCtx.CreateSemaphore());
				dependency.SetWaitPoint(stageFlags);

				from.Node->AddOutputConnection(dependency);
				to.Node->AddInputConnection(dependency);

				return true;
			});
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::SerializeExecutionWavefronts(GenericDraft& builder, const std::vector<Wavefront>& layers)
{
	// each consecutive operation in a wavefront is linked with the every other operation in its surrounding wavefronts
	// if the wavefront doesn't exist, we will skip that particular wavefront and insert the next/previous in its place

	// here we're removing all the empty wavefronts
	std::vector<Wavefront> nonEmptyWavefronts{};
	nonEmptyWavefronts.reserve(layers.size());

	for (const auto& layer : layers)
	{
		if (layer.empty())
			continue;

		nonEmptyWavefronts.emplace_back(layer);
	}

	// no linking if they're all empty
	if (nonEmptyWavefronts.empty())
		return;

	for (size_t i = 1; i < nonEmptyWavefronts.size(); i++)
	{
		const auto& leadingOps = nonEmptyWavefronts[i - 1];
		const auto& followingOps = nonEmptyWavefronts[i];

		for (const auto& leadingOpName : leadingOps)
		{
			for (const auto& followingOpName : followingOps)
			{
				builder.Connect(leadingOpName, followingOpName, vk::PipelineStageFlagBits::eTopOfPipe);
			}
		}
	}
}
