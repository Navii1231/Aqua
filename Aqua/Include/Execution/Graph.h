#pragma once
#include "GraphConfig.h"
#include "GenericNode.h"

AQUA_BEGIN
EXEC_BEGIN

using ExecutionUnit = vkLib::ExecutionUnit;
using GraphList = std::vector<SharedRef<Node>>;
using GraphNodes = std::map<NodeID, SharedRef<Node>>;

template <typename _NodeRefT>
struct BasicGraph
{
	using MyNodeRef = _NodeRefT;
	using MyNodeRefMap = std::map<NodeID, _NodeRefT>;
	using MyNodeTraversalStates = std::map<NodeID, GraphTraversalState>;
	using MyGraphList = std::vector<MyNodeRef>;

	Wavefront InputNodes;
	Wavefront OutputNodes;
	MyNodeRefMap Nodes;

	// keeping track of the node states
	mutable MyNodeTraversalStates TraversalStates;

	SharedRef<std::mutex> Lock;

	void Update() const;
	MyGraphList SortEntries() const;
	std::expected<bool, GraphError> Validate() const;

	// legacy functions we're right now stuck with
	template <typename _Pipeline>
	void InsertPipeOp(NodeID nodeId, const _Pipeline& pipeline);

	template <typename _Op>
	void InsertOperation(vkLib::Context ctx, NodeID nodeId, const _Op& op);

	void ClearInputInjections() const;
	void ClearOutputInjections() const;
	// end of legacy functions

	Node& operator[](NodeID nodeId) { return *Nodes[nodeId]; }
	const Node& operator[](NodeID nodeId) const { return *Nodes.at(nodeId); }

	// external dependencies
	std::expected<bool, GraphError> InjectInputDependencies(const vk::ArrayProxy<DependencyInjection>& injections) const;

	std::expected<bool, GraphError> InjectOutputDependencies(const vk::ArrayProxy<DependencyInjection>& injections) const;

	// Recursive function to generate the sorted array of operations
	void InsertNode(MyGraphList& list, NodeID id, MyNodeRef node) const;
	bool FindClosedCircuit(NodeID id, MyNodeRef node) const;
};

template <typename _NodeRefT>
void AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::Update() const
{
	for (const auto& [name, node] : Nodes)
		node->Update();
}

template <typename _NodeRefT>
typename AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::MyGraphList AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::SortEntries() const
{
	std::lock_guard guard(*Lock);

	MyGraphList list;
	list.reserve(Nodes.size());

	for (auto& node : TraversalStates)
		node.second = GraphTraversalState::ePending;

	for (const auto& path : OutputNodes)
	{
		InsertNode(list, path, Nodes.at(path));
	}

	return list;
}

template <typename _NodeRefT>
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::Validate() const
{
	std::lock_guard guard(*Lock);

	for (const auto& probe : OutputNodes)
	{
		if (FindClosedCircuit(probe, Nodes.at(probe)))
			return std::unexpected(GraphError::eFoundEmbeddedCircuit);

		for (auto& [id, node] : Nodes)
			TraversalStates[id] = GraphTraversalState::ePending;
	}

	return true;
}

template <typename _NodeRefT>
template <typename _Pipeline>
void AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::InsertPipeOp(NodeID nodeId, const _Pipeline& pipeline)
{
	_STL_VERIFY(Nodes.find(nodeId) != Nodes.end(), "Operation doesn't exist");

	Aqua::SharedRef<GenericNode> opRef = std::reinterpret_pointer_cast<GenericNode>(Nodes[nodeId]);

	switch (pipeline.GetPipelineBindPoint())
	{
	case vk::PipelineBindPoint::eGraphics:
		opRef->Type = OpType::eGraphics;
		opRef->GFX = std::reinterpret_pointer_cast<vkLib::GraphicsPipeline>(MakeRef<_Pipeline>(pipeline));
		break;
	case vk::PipelineBindPoint::eCompute:
		opRef->Type = OpType::eCompute;
		opRef->Cmp = std::reinterpret_pointer_cast<vkLib::ComputePipeline>(MakeRef<_Pipeline>(pipeline));
		break;
	case vk::PipelineBindPoint::eRayTracingKHR:
		opRef->Type = OpType::eRayTracing;
		_STL_ASSERT(false, "Ray tracing pipeline is yet to implement in the vkLib");
		break;
	default:
		return;
	}
}

template <typename _NodeRefT>
template <typename _Op>
void AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::InsertOperation(vkLib::Context ctx, NodeID nodeId, const _Op& op)
{
	_STL_VERIFY(Nodes.find(nodeId) != Nodes.end(), "Node doesn't exist");

	Aqua::SharedRef<Node> opRef = MakeRef(op);
	opRef->CloneDependencies(ctx, GetRefAddr(Nodes[nodeId]));

	Nodes[nodeId] = opRef;
}

template <typename _NodeRefT>
void AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::ClearInputInjections() const
{
	for (auto& [name, op] : Nodes)
	{
		op->InputInjections.clear();
	}
}

template <typename _NodeRefT>
void AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::ClearOutputInjections() const
{
	for (auto& [name, op] : Nodes)
	{
		op->OutputInjections.clear();
	}
}

template <typename _NodeRefT>
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::InjectInputDependencies(const vk::ArrayProxy<DependencyInjection>& injections) const
{
	for (const auto& injection : injections)
	{
		if (Nodes.find(injection.ConnectedOp) == Nodes.end())
			return std::unexpected(GraphError::eInjectedOpDoesntExist);
	}

	for (const auto& injection : injections)
	{
		auto op = Nodes.at(injection.ConnectedOp);
		op->AddInputInjection(injection);
	}

	return true;
}

template <typename _NodeRefT>
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::InjectOutputDependencies(const vk::ArrayProxy<DependencyInjection>& injections) const
{
	for (const auto& injection : injections)
	{
		if (Nodes.find(injection.ConnectedOp) == Nodes.end())
			return std::unexpected(GraphError::eInjectedOpDoesntExist);
	}

	for (const auto& injection : injections)
	{
		auto op = Nodes.at(injection.ConnectedOp);
		op->AddOutputInjection(injection);
	}

	return true;
}

template <typename _NodeRefT>
void AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::InsertNode(MyGraphList& list, NodeID id, MyNodeRef node) const
{
	// if the node is already visited, we exit
	if (TraversalStates[id] == GraphTraversalState::eVisited)
		return;

	// visit all incoming connections first
	for (const auto& connection : node->GetInputConnections())
		InsertNode(list, static_cast<NodeID>(*connection), Nodes.at(static_cast<NodeID>(*connection)));

	// otherwise we insert it into the sorted list
	list.emplace_back(node);
	TraversalStates[id] = GraphTraversalState::eVisited;
}

template <typename _NodeRefT>
bool AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::FindClosedCircuit(NodeID id, MyNodeRef node) const
{
	if (TraversalStates[id] == GraphTraversalState::eVisited)
		return false;

	TraversalStates[id] = GraphTraversalState::eVisiting;

	for (const auto& input : node->GetInputConnections())
	{
		// return if we find a closed circuit
		if (TraversalStates[static_cast<NodeID>(*input)] == GraphTraversalState::eVisiting)
			return true;

		if (FindClosedCircuit(static_cast<NodeID>(*input), Nodes.at(static_cast<NodeID>(*input))))
			return true;
	}

	TraversalStates[id] = GraphTraversalState::eVisited;

	return false;
}

template <typename _NodeType1, typename _NodeType2>
_NodeType1& ConvertNode(_NodeType2& node) { return *reinterpret_cast<_NodeType1*>(&node); }

using Graph = BasicGraph<SharedRef<Node>>;

// output layers are already defined in the Graph struct
// if there are n graphs, there will be n - 1 consecutive dependencies, 
// and therefore n - 1 input layers for each graph after the first one
AQUA_API void SerializeExecutionWavefronts(vkLib::Context ctx, const std::vector<Graph>& graphs, const std::vector<Wavefront>& inputLayers);
// enforcing a dependency between every input to each output of consecutive graphs
AQUA_API void SerializeExecutionWavefronts(vkLib::Context ctx, const std::vector<Graph>& graphs);

// removing any dependency between two graphs
AQUA_API void Execute(const vk::ArrayProxy<GraphList>& list, const vk::ArrayProxy<ExecutionUnit>& execUnits);

// waiting
AQUA_API vk::Result WaitFor(const vk::ArrayProxy<ExecutionUnit>& execUnits, bool waitAll = true, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

AQUA_API std::expected<uint32_t, vk::Result> FindFreeExecUnit(const vk::ArrayProxy<ExecutionUnit>& execUnits, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

// cloning
AQUA_API Graph Clone(vkLib::Context ctx, const Graph& graph);

EXEC_END
AQUA_END
