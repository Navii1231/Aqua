#include "NeuralDraft_Literally.h"

std::expected<Aqua::Exec::Graph, Aqua::Exec::GraphError> NeuralDraftAttempt::ConstructForward(const Aqua::Exec::Wavefront& probes)
{
	return _ConstructEx<Aqua::Exec::NodeRef>(probes, true,
		[this](Aqua::Exec::NodeID node, const MyNodeInfo& nodeInfo)->std::expected<Aqua::Exec::NodeRef, Aqua::Exec::GraphError>
		{
			return Aqua::MakeRef<Aqua::Exec::ComputeNode>(node); // return an empty node for now
		}, [this](const NodeInfo& from, const NodeInfo& to, const std::string& depCode)->std::expected<bool, Aqua::Exec::GraphError>
			{
				// constructing nodes

				if (from.NextPaths.empty())
				{
					// reached at the bottom, no dependency injection needed
					return true;
				}

				// need to construct the shader from the dependency connection

				return true;
			});
}

std::expected<Aqua::Exec::Graph, Aqua::Exec::GraphError> NeuralDraftAttempt::ConstructBackward(const Aqua::Exec::Wavefront& probes)
{
	return _ConstructEx<Aqua::Exec::NodeRef>(probes, false,
		[this](Aqua::Exec::NodeID node, const MyNodeInfo& nodeInfo)->std::expected<Aqua::Exec::NodeRef, Aqua::Exec::GraphError>
		{
			return Aqua::MakeRef<Aqua::Exec::ComputeNode>(node); // return an empty node for now
		}, [this](const NodeInfo& from, const NodeInfo& to, const std::string& depCode)->std::expected<bool, Aqua::Exec::GraphError>
			{
				// constructing nodes

				if (from.NextPaths.empty())
				{
					// reached at the bottom, no dependency injection needed
					return true;
				}

				// need to construct the shader from the dependency connection

				return true;
			});
}
