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
	using NodeConstructorFn = std::function<std::expected<_NodeRefT, GraphError>(NodeID, const _NodeInfo&)>;

	template <typename _NodeRefT>
	using NodeConnectorFn = std::function<std::expected<bool, GraphError>(const NodeInfo<_NodeRefT>&, const NodeInfo<_NodeRefT>&, _DepInfo&&...)>;

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
	template <typename _NodeRefT>
	std::expected<BasicGraph<_NodeRefT>, GraphError> _ConstructEx(const Wavefront& probes, bool forward, NodeConstructorFn<_NodeRefT>&& constructor, NodeConnectorFn<_NodeRefT>&& connector) const;

private:
	mutable std::map<NodeID, MyNodeInfo> mNodes;
	MyConnectionList mConnections;

private:
	template <typename _NodeRefT>
	std::expected<bool, GraphError> BuildDependencySkeleton(typename BasicGraph<_NodeRefT>::MyNodeRefMap& opCache, NodeID nodeId, Wavefront& front, MyPaths& paths, NodeConstructorFn<_NodeRefT>& constructor, NodeConnectorFn<_NodeRefT>& connector) const;

	MyPaths ConstructPaths(bool direction) const;
	std::expected<bool, GraphError> ValidateDependencyInputs(MyPaths& paths) const;

	// optimizations... (run automatically once you create a graph)
	template <typename _NodeRefT>
	void RemoveRedundantConnections(BasicGraph<_NodeRefT>& graph) const;

	template <typename _NodeRefT, typename _ConnectorFn, std::_Tuple_like _Tuple, size_t... _Indices>
	static constexpr decltype(auto) ConnectorApply(_ConnectorFn&& _Con, const NodeInfo<_NodeRefT>& from, const NodeInfo<_NodeRefT>& to, _Tuple _tuple, std::index_sequence<_Indices...>)
		noexcept(noexcept(std::invoke(std::forward<_ConnectorFn>(_Con), from, to, std::get<_Indices>(std::forward<_Tuple>(_tuple))...)))
	{
		return std::invoke(std::forward<_ConnectorFn>(_Con), from, to, std::get<_Indices>(std::forward<_Tuple>(_tuple))...);
	}
};

EXEC_END
AQUA_END

template <typename MyNodeInfo, typename ..._DepInfo>
template <typename _NodeRefT>
std::expected<typename AQUA_NAMESPACE::EXEC_NAMESPACE::BasicGraph<_NodeRefT>, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<MyNodeInfo, _DepInfo...>::_ConstructEx(const Wavefront& probes, bool forward, NodeConstructorFn<_NodeRefT>&& constructor, NodeConnectorFn<_NodeRefT>&& connector) const
{
	// TODO: I wonder if this function could detect loops...

	for (auto path : probes)
	{
		if (std::find_if(mNodes.begin(), mNodes.end(), [path](const std::pair<NodeID, MyNodeInfo> nodeInfo) { return path == nodeInfo.first; }) == mNodes.end())
			return std::unexpected(GraphError::ePathDoesntExist);
	}

	MyPaths paths = ConstructPaths(forward);
	auto error = ValidateDependencyInputs(paths);

	if (!error)
		return std::unexpected(error.error());

	typename BasicGraph<_NodeRefT>::MyNodeRefMap nodes;
	Wavefront front;

	for (const auto& path : probes)
	{
		auto error = BuildDependencySkeleton(nodes, path, front, paths, constructor, connector);

		if (!error)
			return std::unexpected(error.error());
	}

	Graph graph;
	graph.InputNodes = front;
	graph.OutputNodes = Wavefront(probes.begin(), probes.end());
	graph.Nodes = nodes;
	graph.Lock = MakeRef<std::mutex>();

	auto validated = graph.Validate();

	if (!validated)
		return std::unexpected(validated.error());

	return graph;
}


template <typename MyNodeInfo, typename ..._DepInfo>
template <typename _NodeRefT>
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<MyNodeInfo, _DepInfo...>::BuildDependencySkeleton(typename BasicGraph<_NodeRefT>::MyNodeRefMap& opCache, NodeID nodeId, Wavefront& front, MyPaths& paths, NodeConstructorFn<_NodeRefT>& constructor, NodeConnectorFn<_NodeRefT>& connector) const
{
	if (opCache.find(nodeId) != opCache.end())
		return true;

	auto constructed = constructor(nodeId, mNodes[nodeId]);

	if (!constructed)
		return std::unexpected(constructed.error());

	opCache[nodeId] = *constructed;

	if (paths[nodeId].empty())
		front.push_back(nodeId);

	for (const auto& connection : paths[nodeId])
	{
		auto success = BuildDependencySkeleton(opCache, connection.Connection.From, front, paths, constructor, connector);

		if (!success)
			return std::unexpected(success.error());

		NodeInfo<_NodeRefT> FromInfo(paths[connection.Connection.From]), ToInfo(paths[nodeId]);

		FromInfo.ID = connection.Connection.From;
		FromInfo.Info = mNodes[FromInfo.ID];
		FromInfo.Node = opCache[FromInfo.ID];

		ToInfo.ID = nodeId;
		ToInfo.Info = mNodes[nodeId];
		ToInfo.Node = *constructed;

		success = ConnectorApply(connector, FromInfo, ToInfo, connection.DepInfo, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<MyDepInfo>>>{});

		if (!success)
			return std::unexpected(success.error());
	}

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
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<MyNodeInfo, _DepInfo...>::ValidateDependencyInputs(MyPaths& paths) const
{
	for (const auto& [id, connectList] : paths)
	{
		for (const auto& con : connectList)
		{
			if (mNodes.find(con.Connection.From) == mNodes.end())
				return std::unexpected(GraphError::eInvalidConnection);

			if (mNodes.find(con.Connection.To) == mNodes.end())
				return std::unexpected(GraphError::eInvalidConnection);

			if (con.Connection.From == con.Connection.To)
				return std::unexpected(GraphError::eDependencyUponItself);
		}
	}

	return true;
}
