#pragma once
#include "GraphConfig.h"

AQUA_BEGIN
EXEC_BEGIN

enum class TraversalState
{
	eSuccess              = 0,
	eQuit                 = 1,
	eSkip                 = 2,
};

enum class EnsembleState
{
	eValid                = 0,
	eIntermediate         = 1,
	eInvalid              = 2,
};

using ExecutionUnit = vkLib::ExecutionUnit;

struct DependencyInjection
{
	std::string Name;
	std::string ConnectedOp;

	vk::PipelineStageFlags WaitPoint = vk::PipelineStageFlagBits::eTopOfPipe;
	vkLib::Core::Ref<vk::Semaphore> Signal;

	void SetName(const std::string& name) { Name = name; }
	void Connect(const std::string& connect) { ConnectedOp = connect; }
	void SetWaitPoint(vk::PipelineStageFlags waitPoint) { WaitPoint = waitPoint; }
	void SetSignal(vkLib::Core::Ref<vk::Semaphore> semaphore) { Signal = semaphore; }
};

struct Dependency
{
	// The connection on which the dependency is formed
	SharedRef<Operation> Incoming;
	SharedRef<Operation> Outgoing;

	vk::PipelineStageFlags WaitPoint;
	vkLib::Core::Ref<vk::Semaphore> Signal;

	void SetIncomingOP(SharedRef<Operation> connection) { Incoming = connection; }
	void SetOutgoingOP(SharedRef<Operation> connection) { Outgoing = connection; }
	void SetWaitPoint(vk::PipelineStageFlags waitPoint) { WaitPoint = waitPoint; }
	void SetSignal(vkLib::Core::Ref<vk::Semaphore> semaphore) { Signal = semaphore; }
};

using OpFn = std::function<void(vk::CommandBuffer, const Operation&)>;
using OpUpdateFn = std::function<void(Operation&)>;

using SemaphoreList = std::vector<vk::Semaphore>;
using PipelineStageList = std::vector<vk::PipelineStageFlags>;

using Wavefront = std::vector<std::string>;

struct Operation
{
	std::string Name;

	// execution data; completely removable if we have Fn as a lambda
	SharedRef<vkLib::GraphicsPipeline> GFX;
	SharedRef<vkLib::ComputePipeline> Cmp;
	//SharedRef<vkLib::RayTracingPipeline> RTX;

	mutable OpStates States;

	OpFn Fn = [](vk::CommandBuffer, const Operation&) {};
	OpUpdateFn UpdateFn = [](Operation&) {}; // could be used to update descriptors

	std::vector<Dependency> InputConnections;  // Operations that must finish before triggering this one
	std::vector<Dependency> OutputConnections; // Operations that can't begin before this one is finished

	std::vector<DependencyInjection> InputInjections; // an external event must finish before this one starts
	std::vector<DependencyInjection> OutputInjections; // an external event is dependent upon this one

	std::uintptr_t OpID = 0; // can be used to uniquely identify the node or as a storage for the user pointer

	void operator()(vk::CommandBuffer cmd, vkLib::Core::Worker executor) const;
	void operator()(vk::CommandBuffer cmd, vkLib::Core::Ref<vkLib::Core::WorkerQueue> worker, vk::Fence fence = nullptr) const;

	void SetOpFn(OpFn&& fn) { Fn = fn; }

	void AddInputConnection(const Dependency& dependency) { InputConnections.emplace_back(dependency); }
	void AddOutputConnection(const Dependency& dependency) { OutputConnections.emplace_back(dependency); }

	void AddInputInjection(const DependencyInjection& inj) { InputInjections.emplace_back(inj); }
	void AddOutputInjection(const DependencyInjection& inj) { OutputInjections.emplace_back(inj); }

	OpType GetOpType() const { return States.Type; }

	AQUA_API std::expected<const vkLib::BasicPipeline*, OpType> GetBasicPipeline() const;
	AQUA_API std::expected<vkLib::BasicPipeline*, OpType> GetBasicPipeline();
	AQUA_API bool Execute(vk::CommandBuffer cmd, vkLib::Core::Worker workers) const;
	AQUA_API bool Execute(vk::CommandBuffer cmd, vkLib::Core::Ref<vkLib::Core::WorkerQueue> worker, vk::Fence fence = nullptr) const;

	AQUA_API vk::SubmitInfo SetupSubmitInfo(vk::CommandBuffer& cmd, SemaphoreList& waitingPoints,
		SemaphoreList& signalList, PipelineStageList& pipelineStages) const;

	Operation() = default;
	Operation(const std::string& name, OpType type)
		: Name(name) { States.Type = type; }
};

using GraphList = std::vector<SharedRef<Operation>>;
using GraphOps = std::unordered_map<std::string, SharedRef<Operation>>;

// Recursive function to generate the sorted array of operations
AQUA_API void InsertNode(GraphList& list, SharedRef<Operation> node);
AQUA_API bool FindClosedCircuit(SharedRef<Operation> node);

struct Graph
{
	Wavefront InputNodes;
	Wavefront OutputNodes;
	GraphOps Nodes;

	SharedRef<std::mutex> Lock;
	vkLib::Context Ctx;

	AQUA_API void Update() const;
	AQUA_API GraphList SortEntries() const;
	AQUA_API std::expected<bool, GraphError> Validate() const;

	AQUA_API void ClearInputInjections() const;
	AQUA_API void ClearOutputInjections() const;

	const Operation& operator[](const std::string& name) const { return *Nodes.at(name); }

	// external dependencies
	AQUA_API std::expected<bool, GraphError> InjectInputDependencies(const vk::ArrayProxy<DependencyInjection>& injections) const;
	AQUA_API std::expected<bool, GraphError> InjectOutputDependencies(const vk::ArrayProxy<DependencyInjection>& injections) const;
};

class Ensemble
{
public:
	using GraphSeq = std::vector<Graph>;
	using EnsembleSeq = std::vector<Ensemble>;
	using SeqVariant = std::variant<GraphSeq, EnsembleSeq>;

public:
	Ensemble() = default;

	// core API
	AQUA_API void Update() const;
	AQUA_API GraphList SortEntries() const;
	AQUA_API std::vector<GraphList> SortEntriesByGroups() const;

	GraphSeq GetGraphs() const { return std::get<GraphSeq>(mVariant); }
	EnsembleSeq GetEnsembleSeq() const { return std::get<EnsembleSeq>(mVariant); }

	const Ensemble& operator[](size_t idx) const { return std::get<EnsembleSeq>(mVariant)[idx]; }
	Ensemble& operator[](size_t idx) { return std::get<EnsembleSeq>(mVariant)[idx]; }

	typename EnsembleSeq::iterator begin() { return std::get<EnsembleSeq>(mVariant).begin(); }
	typename EnsembleSeq::const_iterator begin() const { return std::get<EnsembleSeq>(mVariant).begin(); }
	typename EnsembleSeq::iterator end() { return std::get<EnsembleSeq>(mVariant).end(); }
	typename EnsembleSeq::const_iterator end() const { return std::get<EnsembleSeq>(mVariant).end(); }

	const Graph& Fetch(size_t idx) const { return std::get<GraphSeq>(mVariant)[idx]; }
	Graph& Fetch(size_t idx) { return std::get<GraphSeq>(mVariant)[idx]; }

	EnsembleState GetState() const { return mState; }

	AQUA_API Wavefront GetInputWavefront() const;
	AQUA_API Wavefront GetOutputWavefront() const;

	void SetCtx(vkLib::Context ctx) { mCtx = ctx; }
	void SetSeq(const SeqVariant& seq) { mVariant = seq; }

	// checking out the ensemble content
	bool IsGraphSeq() const { return std::holds_alternative<GraphSeq>(mVariant); }
	bool IsEnsemble() const { return std::holds_alternative<EnsembleSeq>(mVariant); }

	// creates the nodes
	AQUA_API static Ensemble MakeSeq(const GraphSeq& seq);
	AQUA_API static Ensemble MakeSeq(const EnsembleSeq& seq);

	template <typename It>
	static Ensemble MakeSeq(It begin, It end);

	AQUA_API static Ensemble Flatten(const Ensemble& ensemble);
	AQUA_API static Ensemble Heapify(const Ensemble& flatEnsemble, const std::vector<size_t>& cuts);

	template <typename _Ensemble, typename GraphPred, typename EnsPred>
	static TraversalState Traverse(_Ensemble&& ensemble, GraphPred&& graphPred, EnsPred&& ensPred);

	AQUA_API static void UpdateEnsemble(const Ensemble& ensemble);
	AQUA_API static void SortEnsembleEntries(GraphList& entries, const Ensemble& ensemble);

private:
	vkLib::Context mCtx;
	SeqVariant mVariant;

	mutable EnsembleState mState = EnsembleState::eInvalid;

private:
	void SetState(EnsembleState state) const;
};

template <typename It>
Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::MakeSeq(It begin, It end)
{
	return MakeSeq(std::vector(begin, end));
}

template <typename _Ensemble, typename GraphPred, typename EnsPred>
TraversalState Ensemble::Traverse(_Ensemble&& ensemble, GraphPred&& graphPred, EnsPred&& ensPred)
{
	auto state = ensPred(std::forward<_Ensemble>(ensemble));

	if (state != TraversalState::eSuccess)
		return state;

	if (ensemble.IsGraphSeq())
	{
		auto& graphs = std::get<Ensemble::GraphSeq>(ensemble.mVariant);

		for (auto&& graph : graphs)
		{
			auto state = graphPred(graph);

			if (state != TraversalState::eSuccess)
				return state;
		}

		return TraversalState::eSuccess;
	}

	auto& ensembles = std::get<Ensemble::EnsembleSeq>(ensemble.mVariant);

	for (auto&& ensemble : ensembles)
	{
		auto state = Ensemble::Traverse(std::forward<_Ensemble>(ensemble), 
			std::forward<GraphPred>(graphPred), std::forward<EnsPred>(ensPred));

		if (state == TraversalState::eQuit)
			return state;
	}

	return TraversalState::eSuccess;
}


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
AQUA_API Operation Clone(vkLib::Context ctx, const Operation& op);
AQUA_API Graph Clone(vkLib::Context ctx, const Graph& graph);
AQUA_API Ensemble Clone(vkLib::Context ctx, const Ensemble& ensemble);

EXEC_END
AQUA_END
