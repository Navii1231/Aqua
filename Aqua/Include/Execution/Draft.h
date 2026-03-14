#pragma once
#include "GraphConfig.h"
#include "Graph.h"

// for multi-threaded traversal
#include "../Utils/ThreadPool.h"

AQUA_BEGIN
EXEC_BEGIN

using DependencyMap = std::map<NodeID, std::vector<DependencyMetaData>>;

static unsigned int GetHardwareConcurrency()
{ 
	auto maxThreads = std::thread::hardware_concurrency();  
	return maxThreads == 0 ? 1 : maxThreads;
}

// not thread safe except the _ConstructEx function
template <typename _NodeInfo, typename ..._DepInfo>
class Draft
{
public:
	using MyDraftType = Draft<_NodeInfo, _DepInfo...>;

	using MyNodeInfo = _NodeInfo;
	using MyDepInfo = std::tuple<_DepInfo...>;
	using MyMultiTraversalState = std::tuple<std::atomic<GraphTraversalState>, std::atomic<std::thread::id>>;

	struct MySingleTraversalInfo : public std::map<NodeID, GraphTraversalState>
	{
		using Expected = std::expected<bool, GraphError>;
	};

	struct MyMultiTraversalInfo : public std::map<NodeID, MyMultiTraversalState>
	{
		using Expected = Aqua::Future<std::expected<bool, GraphError>>;
	};

	struct Connection
	{
		NodeConnection Connection;
		MyDepInfo DepInfo;
	};

	using MyConnectionList = std::vector<Connection>;
	using MyPaths = std::map<NodeID, MyConnectionList>;

	struct NodeInfo
	{
		NodeID ID;
		const _NodeInfo& Info;
		const MyConnectionList& NextPaths;

		NodeInfo(NodeID id, _NodeInfo& info, const MyConnectionList& paths) : ID(id), Info(info), NextPaths(paths) {}
	};

	template <typename _NodeRefT>
	using NodeConstructorFn = std::function<_NodeRefT(NodeID, const _NodeInfo&)>;

	using NodeConnectorFn = std::function<void(const NodeInfo&, const NodeInfo&, _DepInfo&&...)>;

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

	size_t GetNodeCount() const { return static_cast<size_t>(mNodes.size()); }
	const std::map<NodeID, MyNodeInfo>& GetNodes() const { return mNodes; }

	// construction
	// thread safe only if constructor and connector are thread safe
	// should be able to take any arbitrary scheduler
	template <typename _NodeRefT, typename _NodeConFn, typename _DepConFn>
	std::expected<Wavefront, GraphError> _ConstructEx(const Wavefront& probes, bool forward, _NodeConFn&& constructor, _DepConFn&& connector, int threadCount = 1) const;

private:
	mutable std::map<NodeID, MyNodeInfo> mNodes;
	MyConnectionList mConnections;

private:
	// single threaded version of the construct ex
	// thread safe depending upon whether the constructor and connector are thread safe
	template <typename _NodeRefT, typename _NodeConFn, typename _DepConFn>
	std::expected<Wavefront, GraphError> _ConstructExSingleThreaded(const Wavefront& probes, bool forward, _NodeConFn&& constructor, _DepConFn&& connector) const;

private:
	// dependency tree/DAG building
	template <typename _NodeRefT, typename _TraversalInfo, typename _NodeConFn, typename _DepConFn, typename _Scheduler>
	std::expected<bool, GraphError> BuildDependencySkeleton(std::map<std::thread::id, Wavefront>& front, _TraversalInfo& traversalStates, NodeID nodeId, MyPaths& paths, _NodeConFn& constructor, _DepConFn& connector, _Scheduler& scheduler) const;

	MyPaths ConstructPaths(bool direction) const;
	std::expected<bool, GraphError> ValidateConnections(const Wavefront& probes, MyPaths& paths) const;

	std::expected<MyPaths, GraphError> ConstructAndValidateWavefront(const Wavefront& probes, bool forward) const;

	std::expected<bool, GraphError> CheckIfProbesExist(const Wavefront& probes) const;

	template <typename _ConnectorFn, std::_Tuple_like _Tuple, size_t... _Indices>
	static constexpr decltype(auto) ConnectorApply(_ConnectorFn&& _Con, const NodeInfo& from, const NodeInfo& to, _Tuple _tuple, std::index_sequence<_Indices...>)
		noexcept(noexcept(std::invoke(std::forward<_ConnectorFn>(_Con), from, to, std::get<_Indices>(std::forward<_Tuple>(_tuple))...)))
	{
		return std::invoke(std::forward<_ConnectorFn>(_Con), from, to, std::get<_Indices>(std::forward<_Tuple>(_tuple))...);
	}

private:
	// utility functions
	static bool Compare(GraphTraversalState state, GraphTraversalState compare) { return state == compare; }
	static bool Compare(GraphTraversalState state, std::thread::id compare) { return true; }
	GraphTraversalState Exchange(GraphTraversalState& state, GraphTraversalState value) const { auto prev = state; state = value; return prev; }
	static void Exchange(GraphTraversalState& state, std::thread::id value) {}
	static void Wait(GraphTraversalState& state, GraphTraversalState value) {}
	static void Notify(GraphTraversalState& state) {}

	template <typename _Type>
	static bool Compare(const MyMultiTraversalState& value, const _Type& compare) { return std::get<std::atomic<_Type>>(value).load() == compare; }

	template <typename _Type>
	static _Type Exchange(MyMultiTraversalState& state, const _Type& value) { return std::get<std::atomic<_Type>> (state).exchange(value); }

	static void Wait(MyMultiTraversalState& state, GraphTraversalState value) { std::get<0>(state).wait(value); }

	static void Notify(MyMultiTraversalState& state) { std::get<0>(state).notify_all(); }
};

EXEC_END
AQUA_END

template <typename _NodeInfo, typename ..._DepInfo>
template <typename _NodeRefT, typename _NodeConFn, typename _DepConFn>
std::expected<AQUA_NAMESPACE::EXEC_NAMESPACE::Wavefront, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError>
AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<_NodeInfo, _DepInfo...>::_ConstructEx(const Wavefront& probes, bool forward, _NodeConFn&& constructor, _DepConFn&& connector, int threadCount) const
{
	if (threadCount <= 1)
		return _ConstructExSingleThreaded<_NodeRefT>(probes, forward, constructor, connector);

	ThreadPool pool(threadCount);

	struct MultiThreadedScheduler
	{
		Aqua::Future<std::expected<bool, GraphError>> operator()(std::map<std::thread::id, Wavefront>& info, MyMultiTraversalInfo& traversalStates, NodeID nodeId, MyPaths& paths, _NodeConFn& constructor, _DepConFn& connector, MultiThreadedScheduler& scheduler)
		{
			auto expectedFuture = mPool->EnqueueOnlyIfFree([this, &info, &traversalStates, nodeId, &paths, &constructor, &connector, &scheduler]()
				{
					return mDraft->BuildDependencySkeleton<_NodeRefT>(info, traversalStates, nodeId, paths, constructor, connector, scheduler);
				});

			if (expectedFuture)
				return *expectedFuture;

			std::promise<typename MySingleTraversalInfo::Expected> promise;
			promise.set_value(mDraft->BuildDependencySkeleton<_NodeRefT>(info, traversalStates, nodeId, paths, constructor, connector, scheduler));

			return promise.get_future().share();
		}

		ThreadPool* mPool = nullptr;
		const MyDraftType* mDraft = nullptr;
	} scheduler;

	scheduler.mPool = &pool;
	scheduler.mDraft = this;
	auto pathsErr = ConstructAndValidateWavefront(probes, forward);

	if (!pathsErr)
		return std::unexpected(pathsErr.error());

	auto& paths = *pathsErr;

	MyMultiTraversalInfo traversalStates;

	for (const auto& [id, nodeInfo] : mNodes)
	{
		std::get<0>(traversalStates[id]).store(GraphTraversalState::ePending);
	}

	std::vector<typename MyMultiTraversalInfo::Expected> results{};
	std::map<std::thread::id, Wavefront> fronts{};

	for (auto probe : probes)
	{
		results.emplace_back(scheduler(fronts, traversalStates, probe, paths, constructor, connector, scheduler));
	}

	Wavefront front;

	for (size_t idx = 0; idx < results.size(); idx++)
	{
		const auto& future = results[idx];
		const auto& err = future.get();

		if (!err)
			return std::unexpected(err.error());
	}

	for (const auto& [ID, node] : fronts)
	{
		front.append_range(node);
	}

	return front;
}

template <typename MyNodeInfo, typename ..._DepInfo>
template <typename _NodeRefT, typename _NodeConFn, typename _DepConFn>
std::expected<typename AQUA_NAMESPACE::EXEC_NAMESPACE::Wavefront, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<MyNodeInfo, _DepInfo...>::_ConstructExSingleThreaded(const Wavefront& probes, bool forward, _NodeConFn&& constructor, _DepConFn&& connector) const
{
	auto pathsErr = ConstructAndValidateWavefront(probes, forward);

	if (!pathsErr)
		return std::unexpected(pathsErr.error());

	auto& paths = *pathsErr;

	MySingleTraversalInfo traversalStates;

	for (const auto& [id, nodeInfo] : mNodes)
		traversalStates[id] = GraphTraversalState::ePending;

	struct SingleThreadedScheduler
	{
		std::expected<bool, GraphError> operator()(std::map<std::thread::id, Wavefront>& info, MySingleTraversalInfo& traversalStates, NodeID nodeId, MyPaths& paths, _NodeConFn& constructor, _DepConFn& connector, SingleThreadedScheduler& scheduler)
		{
			return mDraft->BuildDependencySkeleton<_NodeRefT>(info, traversalStates, nodeId, paths, constructor, connector, scheduler);
		}

		const MyDraftType* mDraft = nullptr;
	} scheduler;

	scheduler.mDraft = this;

	std::map<std::thread::id, Wavefront> fronts;

	for (const auto& probe : probes)
	{
		auto error = BuildDependencySkeleton<_NodeRefT>(fronts, traversalStates, probe, paths, constructor, connector, scheduler);

		if (!error)
			return std::unexpected(error.error());
	}

	return fronts[std::this_thread::get_id()];
}

template <typename MyNodeInfo, typename ..._DepInfo>
template <typename _NodeRefT, typename _TraversalInfo, typename _NodeConFn, typename _DepConFn, typename _Scheduler>
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<MyNodeInfo, _DepInfo...>::BuildDependencySkeleton(std::map<std::thread::id, Wavefront>& front, _TraversalInfo& traversalStates, NodeID nodeId, MyPaths& paths, _NodeConFn& constructor, _DepConFn& connector, _Scheduler& scheduler) const
{
	using RetType = std::remove_reference_t<decltype(scheduler(front, traversalStates, nodeId, paths, constructor, connector, scheduler))>;

	if (Compare(traversalStates[nodeId], GraphTraversalState::eVisited))
		return true;

	if (Exchange(traversalStates[nodeId], GraphTraversalState::eVisiting) == GraphTraversalState::eVisiting)
	{
		// same thread runs into the same node again, circular dependency
		if(Compare(traversalStates[nodeId], std::this_thread::get_id()))
			return std::unexpected(GraphError::eFoundEmbeddedCircuit);

		// handle thread collisions
		// we wait for the other thread to finish its
		// execution and then share the results

		Wait(traversalStates[nodeId], GraphTraversalState::eVisiting);

		// this can lead to bottlenecks btw
		return true;
	}

	Exchange(traversalStates[nodeId], std::this_thread::get_id());

	constructor(nodeId, mNodes[nodeId]);

	if (paths[nodeId].empty())
		front[std::this_thread::get_id()].push_back(nodeId);
	
	constexpr bool isMultiThreaded = !std::is_same_v<RetType, std::expected<bool, GraphError>>;
		
	std::vector<RetType> futures;
	std::expected<bool, GraphError> lastState{};

	if constexpr (isMultiThreaded)
	{
		futures.reserve(paths[nodeId].size());
		size_t idx = 0;

		for (const auto& con : paths[nodeId])
		{
			if (con.Connection.From == con.Connection.To)
				return std::unexpected(GraphError::eDependencyUponItself);

			if (idx < paths[nodeId].size() - 1)
				futures.emplace_back(scheduler(front, traversalStates, con.Connection.From, paths, constructor, connector, scheduler));
			else
				lastState = BuildDependencySkeleton<_NodeRefT>(front, traversalStates, con.Connection.From, paths, constructor, connector, scheduler);
			
			idx++;
		}
	}

	size_t idx = 0;

	for (const auto& con : paths[nodeId])
	{
		std::expected<bool, GraphError> err{};
		
		if constexpr (isMultiThreaded)
		{
			if (idx < paths[nodeId].size() - 1)
				err = futures[idx].get();
			else
				err = lastState;
		}
		else
		{
			const auto& err = BuildDependencySkeleton<_NodeRefT>(front, traversalStates, con.Connection.From, paths, constructor, connector, scheduler);
		}

		if (!err)
			return std::unexpected(err.error());

		// constructor and connector determines the overall thread 
		// safety of this routine make them thread safe 
		// for making this routine thread safe

		NodeInfo FromInfo(con.Connection.From, mNodes[con.Connection.From], paths[con.Connection.From]);

		NodeInfo ToInfo(nodeId, mNodes[nodeId], paths[nodeId]);

		ConnectorApply(connector, FromInfo, ToInfo, con.DepInfo, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<MyDepInfo>>>{});

		idx++;
	}

	Exchange(traversalStates[nodeId], GraphTraversalState::eVisited);
	Notify(traversalStates[nodeId]);

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


template <typename _NodeInfo, typename ..._DepInfo>
std::expected<typename AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<_NodeInfo, _DepInfo...>::MyPaths, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<_NodeInfo, _DepInfo...>::ConstructAndValidateWavefront(const Wavefront& probes, bool forward) const
{
	auto valid = CheckIfProbesExist(probes);

	if (!valid)
		return std::unexpected(valid.error());

	MyPaths paths = ConstructPaths(forward);
	valid = ValidateConnections(probes, paths);

	if (!valid)
		return std::unexpected(valid.error());

	return paths;
}

template <typename _NodeInfo, typename ..._DepInfo>
std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::Draft<_NodeInfo, _DepInfo...>::CheckIfProbesExist(const Wavefront& probes) const
{
	for (auto path : probes)
	{
		if (mNodes.find(path) == mNodes.end())
			return std::unexpected(GraphError::ePathDoesntExist);
	}

	return true;
}
