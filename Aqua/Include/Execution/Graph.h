#pragma once
#include "GraphConfig.h"
#include "GenericNode.h"

AQUA_BEGIN
EXEC_BEGIN

using ExecutionUnit = vkLib::ExecutionUnit;

template <typename _NodeRefT>
struct BasicGraph
{
	using MyNodeRef = _NodeRefT;
	using NodeRefMap = std::map<NodeID, _NodeRefT>;
	using MyNodeTraversalStates = std::map<NodeID, GraphTraversalState>;
	using Executable = std::vector<MyNodeRef>;

	Wavefront InputNodes;
	Wavefront OutputNodes;
	NodeRefMap Nodes;

	void Update() const;
	Executable SortEntries() const;

	// legacy functions we're right now stuck with
	template <typename _Pipeline>
	void InsertPipeOp(NodeID nodeId, const _Pipeline& pipeline);

	template <typename _Op>
	void InsertOperation(vkLib::Context ctx, NodeID nodeId, const _Op& op);

	void ClearInputInjections() const;
	void ClearOutputInjections() const;
	// end of legacy functions

	auto& operator[](NodeID nodeId) { return *Nodes[nodeId]; }
	const auto& operator[](NodeID nodeId) const { return *Nodes.at(nodeId); }

	// external dependencies
	std::expected<bool, GraphError> InjectInputDependencies(const vk::ArrayProxy<DependencyInjection>& injections) const;

	std::expected<bool, GraphError> InjectOutputDependencies(const vk::ArrayProxy<DependencyInjection>& injections) const;

	// Recursive function to generate the sorted array of operations
	void InsertNode(MyNodeTraversalStates& traversalState, Executable &list, NodeID id, MyNodeRef node) const;
};

template <typename _NodeRefT, typename _FnPre, typename _FnPost, typename _ChildFn, typename _Scheduler>
TraversalState TraverseExMultiThreaded(const std::map<NodeID, _NodeRefT>& nodes, NodeID id, _FnPre preFn, _FnPost postFn, _ChildFn childFn, _Scheduler& scheduler)
{
	using RetType = std::remove_reference_t<decltype(scheduler(nodes, id, preFn, postFn, childFn, scheduler))>;

	auto state = preFn(id);

	if (state != TraversalState::eSuccess)
		return state;

	std::vector<RetType> futures{};
	TraversalState lastState{};
	constexpr bool isMultiThreaded = !std::is_same_v<RetType, TraversalState>;

	if constexpr (isMultiThreaded)
	{
		futures.reserve(childFn(nodes.at(id)).size());
		size_t idx = 0;
		
		for (auto ID : childFn(nodes.at(id)))
		{
			if(idx < childFn(nodes.at(id)).size() - 1)
				futures.emplace_back(scheduler(nodes, ID, preFn, postFn, childFn, scheduler));
			else
				lastState = TraverseExMultiThreaded(nodes, ID, preFn, postFn, childFn, scheduler);

			idx++;
		}
	}

	size_t idx = 0;

	for (auto ID : childFn(nodes.at(id)))
	{
		TraversalState value{};

		if constexpr (isMultiThreaded)
		{
			if (idx < childFn(nodes.at(id)).size() - 1)
				value = lastState;
			else
				value = futures[idx].get();
		}
		else
			value = TraverseExMultiThreaded(nodes, ID, preFn, postFn, childFn, scheduler);

		if (value == TraversalState::eQuit)
			return value;
	}

	return postFn(id, childFn(nodes.at(id)));
}

template <typename _NodeRefT, typename _FnPre, typename _FnPost, typename _ChildFn>
TraversalState TraverseEx(const std::map<NodeID, _NodeRefT>& nodes, NodeID id, _FnPre preFn, _FnPost postFn, _ChildFn childFn)
{
	struct SingleThreadedScheduler
	{
		TraversalState operator()(const std::map<NodeID, _NodeRefT>& nodes, NodeID id, _FnPre preFn, _FnPost postFn, _ChildFn childFn, SingleThreadedScheduler& scheduler)
		{
			return TraverseExMultiThreaded(nodes, id, preFn, postFn, childFn, scheduler);
		}
	} scheduler;

	return TraverseExMultiThreaded(nodes, id, preFn, postFn, childFn, scheduler);
}

template <typename _NodeRefT, typename _FnPre, typename _FnPost, typename _ChildFn>
TraversalState Traverse(const BasicGraph<_NodeRefT>& graph, _FnPre preFn, _FnPost postFn, _ChildFn childFn, bool forward = false)
{
	auto* wavefront = forward ? &graph.InputNodes : &graph.OutputNodes;
	TraversalState state = TraversalState::eSuccess;

	for (auto ID : *wavefront)
	{
		state = TraverseEx(graph.Nodes, ID, preFn, postFn, childFn);

		if (state == TraversalState::eQuit)
			return state;
	}

	return state;
}

template <typename _NodeRefT, typename _FnPre, typename _FnPost, typename _ChildFn, typename _Scheduler>
TraversalState TraverseMultiThreaded(const BasicGraph<_NodeRefT>& graph, _FnPre preFn, _FnPost postFn, _ChildFn childFn, _Scheduler scheduler, bool forward = false)
{
	auto* wavefront = forward ? &graph.InputNodes : &graph.OutputNodes;

	std::vector<std::remove_reference_t<decltype(scheduler(graph.Nodes, NodeID(), preFn, postFn, childFn, scheduler))>> futures{};

	futures.reserve(wavefront->size());

	for (auto ID : *wavefront)
	{
		futures.emplace_back(scheduler(graph.Nodes, ID, preFn, postFn, childFn, scheduler));
	}

	for (size_t idx = 0; idx < wavefront->size(); idx++)
	{
		auto state = futures[idx].get();

		if (state == TraversalState::eQuit)
			return state;
	}

	return TraversalState::eQuit;
}

template <typename _NodeRefT, typename _CloneFn, typename _ChildFn>
std::tuple<NodeID, TraversalState> CloneEx(std::map<NodeID, _NodeRefT>& nodes, const std::map<NodeID, _NodeRefT>& srcNodes, Aqua::Exec::NodeID id, _CloneFn cloneFn, _ChildFn childFn)
{
	auto [rdm, state] = cloneFn(id);

	if (state != Aqua::Exec::TraversalState::eSuccess)
		return { rdm, state };

	// expecting a Wavefront as the field containing 
	// the children node ids
	auto graphChildren = childFn(srcNodes.at(id));

	auto& children = childFn(nodes[rdm]);
	children.clear();

	for (auto ID : graphChildren)
	{
		auto [childID, childState] = CloneEx(nodes, srcNodes, ID, cloneFn, childFn);

		children.push_back(childID);

		if (childState == TraversalState::eQuit)
			return { rdm, childState };
	}

	return { rdm, state };
}

template <typename _NodeRefT, typename _CloneFn, typename _ChildFn>
Aqua::Exec::BasicGraph<_NodeRefT> Clone(const Aqua::Exec::BasicGraph<_NodeRefT>& calcGraph, _CloneFn cloneFn, _ChildFn childFn)
{
	Aqua::Exec::BasicGraph<_NodeRefT> newGraph{};

	for (auto& IDs : calcGraph.OutputNodes)
	{
		auto [rdm, state] = CloneEx(newGraph.Nodes, calcGraph.Nodes, IDs, [&cloneFn, &newGraph](NodeID id) { return cloneFn(newGraph, id); }, childFn);

		newGraph.OutputNodes.push_back(rdm);
	}

	return newGraph;
}

template <typename _NodeRefT>
void AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::Update() const
{
	for (const auto& [name, node] : Nodes)
		node->Update();
}

template <typename _NodeRefT>
typename AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::Executable AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::SortEntries() const
{
	MyNodeTraversalStates traversalStates;

	for (auto& [id, node] : Nodes)
		traversalStates[id] = GraphTraversalState::ePending;

	Executable list;
	list.reserve(Nodes.size());

	for (const auto& path : OutputNodes)
	{
		InsertNode(traversalStates, list, path, Nodes.at(path));
	}

	return list;
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
void AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>::InsertNode(MyNodeTraversalStates& traversalStates, Executable& list, NodeID id, MyNodeRef node) const
{
	// if the node is already visited, we exit
	if (traversalStates[id] == GraphTraversalState::eVisited)
		return;

	// visit all incoming connections first
	for (const auto& connection : node->GetInputConnections())
		InsertNode(traversalStates, list, static_cast<NodeID>(*connection), Nodes.at(static_cast<NodeID>(*connection)));

	// otherwise we insert it into the sorted list
	list.emplace_back(node);
	traversalStates[id] = GraphTraversalState::eVisited;
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
AQUA_API void Execute(const vk::ArrayProxy<Graph::Executable>& list, const vk::ArrayProxy<ExecutionUnit>& execUnits);

// waiting
AQUA_API vk::Result WaitFor(const vk::ArrayProxy<ExecutionUnit>& execUnits, bool waitAll = true, std::chrono::nanoseconds timeOut = std::chrono::nanoseconds::max());

AQUA_API std::expected<uint32_t, vk::Result> FindFreeExecUnit(const vk::ArrayProxy<ExecutionUnit>& execUnits, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());

// cloning
AQUA_API Graph Clone(vkLib::Context ctx, const Graph& graph);

EXEC_END
AQUA_END
