#pragma once
#include "GraphConfig.h"
#include "Graph.h"

AQUA_BEGIN
EXEC_BEGIN

using DependencyMap = std::map<NodeID, std::vector<DependencyMetaData>>;

template <typename _NodeInfo, typename ..._DepInfo>
class Draft
{
public:
	using MyDraftType = Draft<_NodeInfo, _DepInfo...>;

	using MyNodeInfo = _NodeInfo;
	using MyDepInfo = std::tuple<_DepInfo...>;
	using MyTraversalInfo = std::map<NodeID, GraphTraversalState>;

	struct Connection
	{
		NodeConnection Connection;
		MyDepInfo DepInfo;
	};

	using MyConnectionList = std::vector<Connection>;
	using MyPaths = std::map<NodeID, MyConnectionList>;

	template <typename _NodeRefT>
	struct NodeInfo
	{
		NodeID ID;
		_NodeRefT Node;
		_NodeInfo Info;
		const MyConnectionList& NextPaths;

		NodeInfo(const MyConnectionList& paths) : NextPaths(paths) {}
	};

	template <typename _NodeRefT>
	using NodeConstructorFn = std::function<_NodeRefT(NodeID, const _NodeInfo&)>;

	template <typename _NodeRefT>
	using NodeConnectorFn = std::function<void(const NodeInfo<_NodeRefT>&, const NodeInfo<_NodeRefT>&, _DepInfo&&...)>;

public:
	Draft() = default;
	virtual ~Draft() = default;

	void Clear() { ClearNodes(); ClearDependencies(); }

	void ClearNodes() { mNodes.clear(); }
	void ClearDependencies() { mConnections.clear(); }

	// preparation and submission
	void SubmitNode(NodeID node, const MyNodeInfo& info) { mNodes[node] = info; }
	void RemoveNode(NodeID node) { mNodes.erase(node); }

	MyNodeInfo& operator[](NodeID node) { return mNodes[node]; }
	const MyNodeInfo& operator[](NodeID node) const { return mNodes.at(node); }

	void Connect(NodeID from, NodeID to, _DepInfo&&... depInfos) { mConnections.push_back({ from, to, std::make_tuple<_DepInfo...>(std::forward<_DepInfo>(depInfos)...) }); }

	NodeID GetNodeCount() const { return static_cast<NodeID>(mNodes.size()); }
	const std::map<NodeID, MyNodeInfo> GetNodes() const { return mNodes; }

	// construction
	template <typename _NodeRefT, typename _NodeConFn, typename _DepConFn>
	std::expected<BasicGraph<_NodeRefT>, GraphError> _ConstructEx(const Wavefront& probes, bool forward, _NodeConFn&& constructor, _DepConFn&& connector) const;

private:
	mutable std::map<NodeID, MyNodeInfo> mNodes;
	MyConnectionList mConnections;

private:
	template <typename _NodeRefT, typename _NodeConFn, typename _DepConFn>
	std::expected<bool, GraphError> BuildDependencySkeleton(typename BasicGraph<_NodeRefT>::NodeRefMap& opCache, MyTraversalInfo& traversalStates, NodeID nodeId, Wavefront& front, MyPaths& paths, _NodeConFn& constructor, _DepConFn& connector) const;

	MyPaths ConstructPaths(bool direction) const;
	std::expected<bool, GraphError> ValidateConnections(const Wavefront& probes, MyPaths& paths) const;
	// optimizations... (run automatically once you create a graph)
	void RemoveRedundantConnections(const Wavefront& probes) const;

	template <typename _NodeRefT, typename _ConnectorFn, std::_Tuple_like _Tuple, size_t... _Indices>
	static constexpr decltype(auto) ConnectorApply(_ConnectorFn&& _Con, const NodeInfo<_NodeRefT>& from, const NodeInfo<_NodeRefT>& to, _Tuple _tuple, std::index_sequence<_Indices...>)
		noexcept(noexcept(std::invoke(std::forward<_ConnectorFn>(_Con), from, to, std::get<_Indices>(std::forward<_Tuple>(_tuple))...)))
	{ return std::invoke(std::forward<_ConnectorFn>(_Con), from, to, std::get<_Indices>(std::forward<_Tuple>(_tuple))...); }
};

EXEC_END
AQUA_END

template <typename MyNodeInfo, typename ..._DepInfo>
template <typename _NodeRefT, typename _NodeConFn, typename _DepConFn>
std::expected<typename AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<MyNodeInfo, _DepInfo...>::_ConstructEx(const Wavefront& probes, bool forward, _NodeConFn&& constructor, _DepConFn&& connector) const
{
	for (auto path : probes)
	{
		if (std::find_if(mNodes.begin(), mNodes.end(), [path](const std::pair<NodeID, MyNodeInfo> nodeInfo) { return path == nodeInfo.first; }) == mNodes.end())
			return std::unexpected(GraphError::ePathDoesntExist);
	}

	MyPaths paths = ConstructPaths(forward);
	auto valid = ValidateConnections(probes, paths);

	if (!valid)
		return std::unexpected(valid.error());

	typename BasicGraph<_NodeRefT>::NodeRefMap nodes;
	Wavefront front;
	MyTraversalInfo traversalStates;

	for (const auto& [id, nodeInfo] : mNodes)
		traversalStates[id] = GraphTraversalState::ePending;

	for (const auto& probe : probes)
	{
		auto error = BuildDependencySkeleton(nodes, traversalStates, probe, front, paths, constructor, connector);

		if (!error)
			return std::unexpected(error.error());
	}

	BasicGraph<_NodeRefT> graph;
	graph.InputNodes = front;
	graph.OutputNodes = Wavefront(probes.begin(), probes.end());
	graph.Nodes = nodes;

	return graph;
}


template <typename MyNodeInfo, typename ..._DepInfo>
template <typename _NodeRefT, typename _NodeConFn, typename _DepConFn>
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<MyNodeInfo, _DepInfo...>::BuildDependencySkeleton(typename BasicGraph<_NodeRefT>::NodeRefMap& opCache, MyTraversalInfo& traversalStates, NodeID nodeId, Wavefront& front, MyPaths& paths, _NodeConFn& constructor, _DepConFn& connector) const
{
	if (traversalStates[nodeId] == GraphTraversalState::eVisited)
		return true;

	if (traversalStates[nodeId] == GraphTraversalState::eVisiting)
		return std::unexpected(GraphError::eFoundEmbeddedCircuit);

	traversalStates[nodeId] = GraphTraversalState::eVisiting;

	auto constructed = constructor(nodeId, mNodes[nodeId]);
	opCache[nodeId] = constructed;

	if (paths[nodeId].empty())
		front.push_back(nodeId);

	for (const auto& con : paths[nodeId])
	{
		if (con.Connection.From == con.Connection.To)
			return std::unexpected(GraphError::eDependencyUponItself);

		auto success = BuildDependencySkeleton(opCache, traversalStates, con.Connection.From, front, paths, constructor, connector);

		if (!success)
			return std::unexpected(success.error());

		NodeInfo<_NodeRefT> FromInfo(paths[con.Connection.From]), ToInfo(paths[nodeId]);

		FromInfo.ID = con.Connection.From;
		FromInfo.Info = mNodes[FromInfo.ID];
		FromInfo.Node = opCache[FromInfo.ID];

		ToInfo.ID = nodeId;
		ToInfo.Info = mNodes[nodeId];
		ToInfo.Node = constructed;

		ConnectorApply(connector, FromInfo, ToInfo, con.DepInfo, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<MyDepInfo>>>{});
	}

	traversalStates[nodeId] = GraphTraversalState::eVisited;
	return true;
}

template <typename _NodeInfo, typename ..._DepInfo>
typename AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<_NodeInfo, _DepInfo...>::MyPaths AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<_NodeInfo, _DepInfo...>::ConstructPaths(bool direction) const
{
	MyPaths paths{};
	
	std::for_each(mConnections.begin(), mConnections.end(), [&paths, direction](const Connection& connection)
		{
			if (direction)
			{
				auto& connectList = paths[connection.Connection.To];
				connectList.push_back(connection);

				if (connectList.capacity() == connectList.size())
					connectList.reserve(2 * connectList.size());
			}
			else
			{
				auto& connectList = paths[connection.Connection.From];
				connectList.push_back(connection);

				if (connectList.capacity() == connectList.size())
					connectList.reserve(2 * connectList.size());
			}
		});

	return paths;
}

template <typename MyNodeInfo, typename ..._DepInfo> 
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<MyNodeInfo, _DepInfo...>::ValidateConnections(const Wavefront& probes, MyPaths& paths) const
{
	// todo: O(n*logn) time complexity but the checks are absolutely necessary...

	for (const auto& probe : probes)
	{
		for (const auto& con : paths[probe])
		{
			if (mNodes.find(con.Connection.From) == mNodes.end())
				return std::unexpected(GraphError::eInvalidConnection);

			if (mNodes.find(con.Connection.To) == mNodes.end())
				return std::unexpected(GraphError::eInvalidConnection);
		}
	}

	return true;
}
