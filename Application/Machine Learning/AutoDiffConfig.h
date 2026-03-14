#pragma once
#include "Config.h"
#include "Utils/Random.h"
#include "Execution/Graph.h"

enum class CalcNodeType
{
	eInvalid                  = -1,
	eAccumulation             = 1,
	eExpansion                = 2,
	eDifferential             = 3,
	eFunction                 = 4,
};

template <typename _Info, typename ..._Types>
struct CalcNode
{
	using MyIDType = Aqua::Exec::NodeID;
	using MyInfo = _Info;
	using MyVariant = std::variant<_Types...>;
	using MyChildrenType = std::vector<MyIDType>;

	MyIDType mNodeID;
	CalcNodeType mCalType = CalcNodeType::eInvalid;
	MyInfo mInfo;
	MyVariant mVar;

	MyChildrenType mChildren;

	CalcNode() = default;
	CalcNode(MyIDType id) : mNodeID(id) {}

	MyIDType GetNodeID() const { return mNodeID; }
	CalcNodeType GetCalcType() const { return mCalType; }
	const MyInfo& GetInfo() const { return mInfo; }
	const MyVariant& GetVariant() const { return mVar; }

	const MyChildrenType& GetChildren() const { return mChildren; }

	void SetNodeID(MyIDType id) { mNodeID = id; }
	void SetInfo(const MyInfo& info) { mInfo = info; }
	void SetCalType(CalcNodeType type) { mCalType = type; }
	void SetVariant(const MyVariant& var) { mVar = var; }

	void SetChildren(const MyChildrenType& children) { mChildren = children; }
};

template <typename ..._Types>
using CalcGraph = Aqua::Exec::BasicGraph<CalcNode<_Types...>>;

template <typename _Info, typename ..._Types>
typename CalcNode<_Info, _Types...>::MyChildrenType& GetChildren(CalcNode<_Info, _Types...>& node)
{
	return node.mChildren;
}

template <typename _Info, typename ..._Types>
typename const CalcNode<_Info, _Types...>::MyChildrenType& GetChildren(const CalcNode<_Info, _Types...>& node)
{
	return node.mChildren;
}

template <typename _NodeRefT>
auto& GetExprNodeChildren(_NodeRefT node)
{
	return GetChildren(*node);
}

template <typename _NodeRefT>
std::pair<Aqua::Exec::NodeID, Aqua::Exec::TraversalState> CloneCalcNodeByRef(Aqua::Exec::BasicGraph<_NodeRefT>& dist, const Aqua::Exec::BasicGraph<_NodeRefT>& src, Aqua::Exec::NodeID srcID) 
{ 
	dist.Nodes[srcID] = src.Nodes.at(srcID);

	if (std::find(src.InputNodes.begin(), src.InputNodes.end(), srcID) != src.InputNodes.end())
		dist.InputNodes.push_back(srcID);

	return { srcID, Aqua::Exec::TraversalState::eSuccess };
}

template <typename _RNG, typename _NodeRefT>
std::pair<Aqua::Exec::NodeID, Aqua::Exec::TraversalState> CloneCalcNodeByValue(const _RNG& rng, Aqua::Exec::BasicGraph<_NodeRefT>& dist, const Aqua::Exec::BasicGraph<_NodeRefT>& src, Aqua::Exec::NodeID srcID)
{
	Aqua::Exec::NodeID distID = rng();

	dist.Nodes[distID] = Aqua::MakeRef(src[srcID]);
	dist[distID].SetNodeID(distID);

	if (std::find(src.InputNodes.begin(), src.InputNodes.end(), srcID) != src.InputNodes.end())
		dist.InputNodes.push_back(distID);

	return { distID, Aqua::Exec::TraversalState::eSuccess };
}

template <typename _NodeRefT>
auto CloneByRef(const Aqua::Exec::BasicGraph<_NodeRefT>& calcGraph)
{
	return Aqua::Exec::Clone(calcGraph, std::bind(CloneCalcNodeByRef<_NodeRefT>, std::placeholders::_1, calcGraph, std::placeholders::_2), GetExprNodeChildren<_NodeRefT>);
}

template <typename _NodeRefT>
auto CloneExByRef(Aqua::Exec::BasicGraph<_NodeRefT>& graph, const Aqua::Exec::BasicGraph<_NodeRefT>& calcGraph, Aqua::Exec::NodeID srcID)
{
	return Aqua::Exec::CloneEx(graph.Nodes, calcGraph.Nodes, srcID, [&graph, &calcGraph](Aqua::Exec::NodeID srcID) { return CloneCalcNodeByRef(graph, calcGraph, srcID); }, GetExprNodeChildren<_NodeRefT>);
}

template <typename _RNG, typename _NodeRefT>
auto CloneByValue(const _RNG& rng, const Aqua::Exec::BasicGraph<_NodeRefT>& calcGraph)
{
	return Aqua::Exec::Clone(calcGraph, [&calcGraph, &rng](Aqua::Exec::BasicGraph<_NodeRefT>& graph, Aqua::Exec::NodeID srcID) { return CloneCalcNodeByValue(rng, graph, calcGraph, srcID); }, GetExprNodeChildren<_NodeRefT>);
}

template <typename _RNG, typename _NodeRefT>
auto CloneExByValue(const _RNG& rng, Aqua::Exec::BasicGraph<_NodeRefT>& graph, const Aqua::Exec::BasicGraph<_NodeRefT>& calcGraph, Aqua::Exec::NodeID srcID)
{
	return Aqua::Exec::CloneEx(graph.Nodes, calcGraph.Nodes, srcID, [&graph, &calcGraph, &rng](Aqua::Exec::NodeID srcID) { return CloneCalcNodeByValue(rng, graph, calcGraph, srcID); }, GetExprNodeChildren<_NodeRefT>);
}

template <typename _Pred, typename ..._Types>
Aqua::Exec::TraversalState TraverseCalcGraphImpl(CalcGraph<_Types...>& graph, Aqua::Exec::NodeID id, const _Pred& pred, bool postTraversal)
{
	if(!postTraversal)
	{
		Aqua::Exec::TraversalState state = pred(graph[id]);

		if (state != Aqua::Exec::TraversalState::eSuccess)
			return state;
	}

	for (auto childID : graph[id].mChildren)
	{
		auto state = TraverseCalcGraphImpl<_Pred, _Types...>(graph, childID, pred, postTraversal);

		if (state == Aqua::Exec::TraversalState::eQuit)
			return state;
	}

	if (postTraversal)
	{
		Aqua::Exec::TraversalState state = pred(graph[id]);

		if (state != Aqua::Exec::TraversalState::eSuccess)
			return state;
	}

	return Aqua::Exec::TraversalState::eSuccess;
}
