#include "Core/Aqpch.h"
#include "Execution/Graph.h"

void AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::operator()(vk::CommandBuffer cmds, vkLib::Core::Worker executor) const
{
	States.Exec = State::eExecute;

	Fn(cmds, *this);
	Execute(cmds, executor);

	States.Exec = State::eReady;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::operator()(vk::CommandBuffer cmds, 
	vkLib::Core::Ref<vkLib::Core::WorkerQueue> worker, vk::Fence fence) const
{
	States.Exec = State::eExecute;

	Fn(cmds, *this);
	Execute(cmds, worker, fence);

	States.Exec = State::eReady;
}

std::expected<const vkLib::BasicPipeline*, AQUA_NAMESPACE::EXEC_NAMESPACE::OpType> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::GetBasicPipeline() const
{
	switch (GetOpType())
	{
	case OpType::eCompute:
		return Cmp.get();
	case OpType::eGraphics:
		return GFX.get();
	case OpType::eRayTracing:
		return std::unexpected(GetOpType());
	default:
		return std::unexpected(GetOpType());
	}
}

std::expected<vkLib::BasicPipeline*, AQUA_NAMESPACE::EXEC_NAMESPACE::OpType> AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::GetBasicPipeline()
{
	switch (GetOpType())
	{
	case OpType::eCompute:
		return Cmp.get();
	case OpType::eGraphics:
		return GFX.get();
	case OpType::eRayTracing:
		return std::unexpected(GetOpType());
	default:
		return std::unexpected(GetOpType());
	}
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::Execute(vk::CommandBuffer cmd, vkLib::Core::Worker workers) const
{
	SemaphoreList waitingList, signalList;
	PipelineStageList pipelineStages;

	vk::SubmitInfo submitInfo = SetupSubmitInfo(cmd, waitingList, signalList, pipelineStages);
	workers.Dispatch(submitInfo);

	return true;
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::Execute(vk::CommandBuffer cmd, 
	vkLib::Core::Ref<vkLib::Core::WorkerQueue> worker, vk::Fence fence) const
{
	SemaphoreList waitingList, signalList;
	PipelineStageList pipelineStages;

	vk::SubmitInfo submitInfo = SetupSubmitInfo(cmd, waitingList, signalList, pipelineStages);
	worker->Submit(submitInfo, fence);

	return true;
}

vk::SubmitInfo AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::SetupSubmitInfo(vk::CommandBuffer& cmd,
	SemaphoreList& waitingList, SemaphoreList& signalList, PipelineStageList& pipelineStages) const
{
	for (const auto& input : InputConnections)
	{
		pipelineStages.emplace_back(input.WaitPoint);
		waitingList.emplace_back(*input.Signal);
	}

	for (const auto& input : InputInjections)
	{
		pipelineStages.emplace_back(input.WaitPoint);
		waitingList.emplace_back(*input.Signal);
	}

	for (const auto& output : OutputConnections)
	{
		signalList.emplace_back(*output.Signal);
	}

	for (const auto& output : OutputInjections)
	{
		signalList.emplace_back(*output.Signal);
	}

	vk::SubmitInfo submitInfo{};

	submitInfo.setCommandBuffers(cmd);
	submitInfo.setSignalSemaphores(signalList);
	submitInfo.setWaitSemaphores(waitingList);
	submitInfo.setWaitDstStageMask(pipelineStages);

	return submitInfo;
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::FindClosedCircuit(SharedRef<Operation> node)
{
	if (node->States.TraversalState == GraphTraversalState::eVisited)
		return false;

	node->States.TraversalState = GraphTraversalState::eVisiting;

	for (const auto& input : node->InputConnections)
	{
		// return if we find a closed circuit
		if (input.Incoming->States.TraversalState == GraphTraversalState::eVisiting)
			return true;

		if (FindClosedCircuit(input.Incoming))
			return true;
	}

	node->States.TraversalState = GraphTraversalState::eVisited;

	return false;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::SerializeExecutionWavefronts(vkLib::Context ctx, const std::vector<Graph>& graphs, const std::vector<Wavefront>& inputLayers)
{
	for (size_t i = 0; i < graphs.size() - 1; i++)
	{
		const auto& leadingOps = graphs[i].OutputNodes;
		const auto& followingOps = inputLayers[i];

		for (const auto& leadingOpName : leadingOps)
		{
			for (const auto& followingOpName : followingOps)
			{
				auto semaphore = ctx.CreateSemaphore();

				DependencyInjection inInj;
				inInj.SetName(followingOpName);
				inInj.Connect(leadingOpName);
				inInj.SetSignal(semaphore);
				inInj.SetWaitPoint(vk::PipelineStageFlagBits::eTopOfPipe);

				_STL_VERIFY(graphs[i].InjectOutputDependencies(inInj), "Couldn't serialize the execution graph");

				DependencyInjection outInj;
				outInj.SetName(leadingOpName);
				outInj.Connect(followingOpName);
				outInj.SetSignal(semaphore);
				outInj.SetWaitPoint(vk::PipelineStageFlagBits::eTopOfPipe);

				_STL_VERIFY(graphs[i + 1].InjectInputDependencies(outInj), "Couldn't serialize the execution graph");
			}
		}
	}
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::SerializeExecutionWavefronts(vkLib::Context ctx, const std::vector<Graph>& graphs)
{
	std::vector<Wavefront> allInputs{};
	allInputs.reserve(graphs.size());

	for (uint32_t i = 1; i < graphs.size(); i++)
	{
		allInputs.emplace_back(graphs[i].InputNodes);
	}

	SerializeExecutionWavefronts(ctx, graphs, allInputs);
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Execute(const vk::ArrayProxy<GraphList>& lists, const vk::ArrayProxy<ExecutionUnit>& execUnits)
{
	size_t execCount = execUnits.size();

	if (execCount == 0)
		return;

	size_t Idx = 0;

	for (const auto& execList : lists)
	{
		for (const auto& nodeRef : execList)
		{
			size_t unitIdx = (Idx++) % execCount;
			auto& node = *nodeRef;
			const ExecutionUnit* units = execUnits.data();

			units[unitIdx].Worker.WaitIdle();
			node(units[unitIdx].CmdBufs, units[unitIdx].Worker);
		}
	}
}

vk::Result AQUA_NAMESPACE::EXEC_NAMESPACE::WaitFor(const vk::ArrayProxy<ExecutionUnit>& execUnits, bool waitAll /*= true*/, std::chrono::nanoseconds timeOut /*= std::chrono::nanoseconds::max()*/)
{
	return vkLib::WaitForExecUnits(execUnits, waitAll, timeOut);
}

std::expected<uint32_t, vk::Result> AQUA_NAMESPACE::EXEC_NAMESPACE::FindFreeExecUnit(const vk::ArrayProxy<ExecutionUnit>& execUnits, std::chrono::nanoseconds timeout /*= std::chrono::nanoseconds::max()*/)
{
	vk::Result success = AQUA_NAMESPACE::EXEC_NAMESPACE::WaitFor(execUnits, false, timeout);

	if (success != vk::Result::eSuccess)
		return std::unexpected(success);

	uint32_t idx = 0;

	for (auto unit : execUnits)
	{
		if (vkLib::WaitForWorkers(unit.Worker, true, std::chrono::nanoseconds(0)) == vk::Result::eSuccess)
			return idx;

		idx++;
	}

	// recycle this error if the exec unit is used again in another thread in the meantime
	return std::unexpected(vk::Result::eErrorOutOfDateKHR);
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Operation AQUA_NAMESPACE::EXEC_NAMESPACE::Clone(vkLib::Context ctx, const Operation& op)
{
	Operation cloned = op;

	if (cloned.Cmp)
		cloned.Cmp = MakeRef(Clone(ctx, *cloned.Cmp));

	if (cloned.GFX)
		cloned.GFX = MakeRef(Clone(ctx, *cloned.GFX));

	for (auto& dependency : cloned.InputConnections)
	{ dependency.SetSignal(ctx.CreateSemaphore()); }

	for (auto& dependency : cloned.OutputConnections)
	{ dependency.SetSignal(ctx.CreateSemaphore()); }

	for (auto& dependency : cloned.InputInjections)
	{ dependency.SetSignal(ctx.CreateSemaphore()); }

	for (auto& dependency : cloned.OutputInjections)
	{ dependency.SetSignal(ctx.CreateSemaphore()); }

	return cloned;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Graph AQUA_NAMESPACE::EXEC_NAMESPACE::Clone(vkLib::Context ctx, const Graph& graph)
{
	Graph cloned = graph;

	cloned.Ctx = ctx;
	cloned.Lock = MakeRef<std::mutex>();
	
	// todo: here we clone all the nodes while respecting the dependencies
	
	for (auto& [name, node] : cloned.Nodes)
	{
		*node = Clone(ctx, *node);
	}

	for (auto& [name, node] : cloned.Nodes)
	{
		// set the input and output dependency nodes

		for (auto& dependency : node->InputConnections)
		{
			dependency.Incoming = cloned.Nodes[dependency.Incoming->Name];
			dependency.Outgoing = node;
		}

		for (auto& dependency : node->OutputConnections)
		{
			dependency.Incoming = node;
			dependency.Outgoing = cloned.Nodes[dependency.Outgoing->Name];
		}
	}

	return cloned;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Clone(vkLib::Context ctx, const Ensemble& ensemble)
{
	Ensemble cloned = ensemble;

	Ensemble::Traverse(cloned, [&ctx](Graph& graph)
		{
			graph = Clone(ctx, graph);
			return TraversalState::eSuccess;
		}, [&ctx](Ensemble& ensemble)
			{
				return TraversalState::eSuccess;
			});

	return cloned;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::InsertNode(GraphList& list, SharedRef<Operation> node)
{
	// if the node is already visited, we exit
	if (node->States.TraversalState == GraphTraversalState::eVisited)
		return;

	// visit all incoming connections first
	for (auto& connection : node->InputConnections)
		InsertNode(list, connection.Incoming);

	// otherwise we insert it into the sorted list
	list.emplace_back(node);
	node->States.TraversalState = GraphTraversalState::eVisited;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::Update() const
{
	for (const auto& [name, node] : Nodes)
		node->UpdateFn(*node);
}

AQUA_NAMESPACE::EXEC_NAMESPACE::GraphList AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::SortEntries() const
{
	std::lock_guard guard(*Lock);

	GraphList list;
	list.reserve(Nodes.size());

	for (auto& node : Nodes)
		node.second->States.TraversalState = GraphTraversalState::ePending;

	for (const auto& path : OutputNodes)
	{
		InsertNode(list, Nodes.at(path));
	}

	return list;
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::Validate() const
{
	for (const auto& probe : OutputNodes)
	{
		if (FindClosedCircuit(Nodes.at(probe)))
			return std::unexpected(GraphError::eFoundEmbeddedCircuit);

		for (auto& node : Nodes)
			node.second->States.TraversalState = GraphTraversalState::ePending;
	}

	return true;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::ClearInputInjections() const
{
	for (auto& [name, op] : Nodes)
	{
		op->InputInjections.clear();
	}
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::ClearOutputInjections() const
{
	for (auto& [name, op] : Nodes)
	{
		op->OutputInjections.clear();
	}
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::InjectInputDependencies(
		const vk::ArrayProxy<DependencyInjection>& injections) const
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

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::InjectOutputDependencies(
	const vk::ArrayProxy<DependencyInjection>& injections) const
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

void AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::Update() const
{
	UpdateEnsemble(*this);
}

AQUA_NAMESPACE::EXEC_NAMESPACE::GraphList AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::SortEntries() const
{
	GraphList entries;
	SortEnsembleEntries(entries, *this);

	return entries;
}

std::vector<AQUA_NAMESPACE::EXEC_NAMESPACE::GraphList> AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::SortEntriesByGroups() const
{
	std::vector<GraphList> entryGroups;

	Traverse(*this, [](const Graph& graph)
		{
			return TraversalState::eSuccess;
		}, [&entryGroups](const Ensemble& ensemble)
			{
				if (!ensemble.IsGraphSeq())
					return TraversalState::eSuccess;

				if (entryGroups.size() >= entryGroups.capacity())
					entryGroups.reserve(2 * entryGroups.size());

				entryGroups.emplace_back(ensemble.SortEntries());
				return TraversalState::eSuccess; 
			});

	entryGroups.shrink_to_fit();

	return entryGroups;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Wavefront AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::GetInputWavefront() const
{
	Wavefront inputWavefront;

	Traverse(*this, [&inputWavefront](const Graph& graph)
		{
			inputWavefront = graph.InputNodes;

			return TraversalState::eQuit; // quit once the input is found
		}, [](const Ensemble&) { return TraversalState::eSuccess; });

	return inputWavefront;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Wavefront AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::GetOutputWavefront() const
{
	Wavefront outWavefront;

	Traverse(*this, [&outWavefront](const Graph& graph)
		{
			outWavefront = graph.InputNodes;
			return TraversalState::eSuccess;
		}, [](const Ensemble&) { return TraversalState::eSuccess; });

	return outWavefront;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::MakeSeq(const GraphSeq& seq)
{
	Ensemble ensemble;
	ensemble.mState = seq.empty() ? EnsembleState::eIntermediate : EnsembleState::eValid;

	vkLib::Context ctx = seq.front().Ctx;

	// TODO: need a check where we must make sure all the graphs have the same context

	Aqua::Exec::SerializeExecutionWavefronts(ctx, seq);

	ensemble.SetCtx(ctx);
	ensemble.SetSeq(seq);

	return ensemble;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::MakeSeq(const EnsembleSeq& seq)
{
	Ensemble ensemble;

	vkLib::Context ctx{};

	// finding the context and making sure all DAGs have the same context
	for (auto ensembleRef : seq)
	{
		Traverse(ensembleRef, [&ctx, &ensemble](const Graph& graph)
			{
				ctx = graph.Ctx;
				ensemble.mState = EnsembleState::eValid;
				return TraversalState::eQuit;
			}, [](const Ensemble&) { return TraversalState::eSuccess; });
	}

	ensemble.SetCtx(ctx);
	ensemble.SetSeq(seq);

	return ensemble;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::Flatten(const Ensemble& ensemble)
{
	GraphSeq mergedGraphs;

	if (ensemble.mState != EnsembleState::eValid)
		throw std::runtime_error("Can't flatten an intermediate/invalid ensemble");

	if (ensemble.IsGraphSeq())
		return ensemble;
	else if (ensemble.IsEnsemble())
	{
		// recursively flatten child ensembles
		for (const auto& child : ensemble.GetEnsembleSeq())
		{
			uint32_t splitIdx = static_cast<uint32_t>(mergedGraphs.size());
			const auto joinedChild = Flatten(child);
			child.mState = EnsembleState::eInvalid;
			mergedGraphs.append_range(joinedChild.GetGraphs());

			if (splitIdx != 0)
			{
				// forming new semaphore barriers
				SerializeExecutionWavefronts(ensemble.mCtx, { mergedGraphs[splitIdx - 1], mergedGraphs[splitIdx] });
			}
		}
	}

	ensemble.mState = EnsembleState::eIntermediate;

	Ensemble flattened{};
	flattened.mState = EnsembleState::eValid;
	flattened.SetSeq(mergedGraphs);

	return flattened;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::Heapify(const Ensemble& flatEnsemble, const std::vector<size_t>& cuts)
{
	// Must be flat
	if (!flatEnsemble.IsGraphSeq())
		throw std::runtime_error("Heapify expects a flattened leaf ensemble.");

	if (flatEnsemble.mState != EnsembleState::eValid)
		throw std::runtime_error("Can't heapify an intermediate/invalid ensemble");

	const auto& flatSeq = flatEnsemble.GetGraphs();
	std::vector<Ensemble::GraphSeq> regions;
	regions.reserve(cuts.size() + 1);

	size_t start = 0;
	for (size_t cut : cuts)
	{
		if (cut > flatSeq.size())
			throw std::out_of_range("Cut index exceeds flat sequence size.");

		Ensemble::GraphSeq region(flatSeq.begin() + start, flatSeq.begin() + cut);
		regions.push_back(std::move(region));
		start = cut;

	}

	// Add remaining graphs after the last cut
	if (start < flatSeq.size())
	{
		Ensemble::GraphSeq region(flatSeq.begin() + start, flatSeq.end());
		regions.push_back(std::move(region));
	}

	// Convert each region into a leaf ensemble
	Ensemble::EnsembleSeq children;
	children.reserve(regions.size());

	for (auto& region : regions)
	{
		// The boundaries now should have no injections
		region.front().ClearInputInjections();
		region.back().ClearOutputInjections();

		children.push_back(Ensemble::MakeSeq(region));
	}

	// Form new intermediate ensemble (depth incremented)
	flatEnsemble.mState = EnsembleState::eInvalid;

	Ensemble root;
	root.mVariant = children;
	root.mState = EnsembleState::eValid;

	return root;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::UpdateEnsemble(const Ensemble& ensemble)
{
	Ensemble::Traverse(ensemble, [](const Graph& graph)
		{
			graph.Update();
			return TraversalState::eSuccess;
		}, [](const Ensemble& ensemble) { return TraversalState::eSuccess; });
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::SortEnsembleEntries(GraphList& entries, const Ensemble& ensemble)
{
	Ensemble::Traverse(ensemble, [&entries](const Graph& graph)
		{
			entries.append_range(graph.SortEntries());
			return TraversalState::eSuccess;
		}, [](const Ensemble& ensemble) { return TraversalState::eSuccess; });
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::SetState(EnsembleState state) const
{
	Ensemble::Traverse(*this, [](const Graph& graph) {return TraversalState::eSuccess; },
		[state](const Ensemble& ensemble)
		{
			ensemble.mState = state;
			return TraversalState::eSuccess;
		});
}
