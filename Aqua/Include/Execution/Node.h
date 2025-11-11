#pragma once
#include "GraphConfig.h"
#include "Utils.h"

AQUA_BEGIN
EXEC_BEGIN

struct NodeConnection
{
	NodeID From, To;

	NodeID operator*() const { return From; }
};

struct DependencyInjection
{
	NodeID CurrOp;
	NodeID ConnectedOp;

	vk::PipelineStageFlags WaitPoint = vk::PipelineStageFlagBits::eTopOfPipe;
	vkLib::Core::Ref<vk::Semaphore> Signal;

	void SetCurrOp(NodeID curr) { CurrOp = curr; }
	void Connect(NodeID connect) { ConnectedOp = connect; }
	void SetWaitPoint(vk::PipelineStageFlags waitPoint) { WaitPoint = waitPoint; }
	void SetSignal(vkLib::Core::Ref<vk::Semaphore> semaphore) { Signal = semaphore; }
};

struct Dependency : public NodeConnection
{
	vk::PipelineStageFlags WaitPoint;
	vkLib::Core::Ref<vk::Semaphore> Signal;

	void SetIncomingOP(NodeID connection) { From = connection; }
	void SetOutgoingOP(NodeID connection) { To = connection; }
	void SetWaitPoint(vk::PipelineStageFlags waitPoint) { WaitPoint = waitPoint; }
	void SetSignal(vkLib::Core::Ref<vk::Semaphore> semaphore) { Signal = semaphore; }

	NodeID GetIncoming() const { return From; }
	NodeID GetOutgoing() const { return To; }
};

using SemaphoreList = std::vector<vk::Semaphore>;
using PipelineStageList = std::vector<vk::PipelineStageFlags>;

using Wavefront = std::vector<NodeID>;
using UnorderedResourceDescMap = std::unordered_map<vkLib::DescriptorLocation, SharedRef<GraphRsc>, DescriptorHasher>;
using ResourceDescMap = std::map<size_t, SharedRef<GraphRsc>>;

// revolution ongoing...
struct Node
{
	NodeID NodeId;

	// Node states...
	mutable State ExecState = State::ePending;
	mutable GraphTraversalState TraversalState = GraphTraversalState::ePending;

	std::vector<Dependency> InputConnections;  // Operations that must finish before triggering this one
	std::vector<Dependency> OutputConnections; // Operations that can't begin before this one is finished

	std::vector<DependencyInjection> InputInjections; // an external event must finish before this one starts
	std::vector<DependencyInjection> OutputInjections; // an external event is dependent upon this one

	// essential execution utilities to exec::node
	void operator()(vk::CommandBuffer cmd, vkLib::Core::Worker executor) const;
	void operator()(vk::CommandBuffer cmd, vkLib::Core::Ref<vkLib::Core::WorkerQueue> worker, vk::Fence fence = nullptr) const;

	void AddInputConnection(const Dependency& dependency) { InputConnections.emplace_back(dependency); }
	void AddOutputConnection(const Dependency& dependency) { OutputConnections.emplace_back(dependency); }

	void AddInputInjection(const DependencyInjection& inj) { InputInjections.emplace_back(inj); }
	void AddOutputInjection(const DependencyInjection& inj) { OutputInjections.emplace_back(inj); }

	// execution functions
	virtual std::expected<const vkLib::BasicPipeline*, OpType> GetBasicPipeline() const { return nullptr; }
	virtual std::expected<vkLib::BasicPipeline*, OpType> GetBasicPipeline() { return nullptr; }
	virtual bool Execute(vk::CommandBuffer cmd) const { return false; /* do nothing */ }
	virtual bool Update() { return false; /* do nothing */ }

	// submission
	AQUA_API vk::SubmitInfo SetupSubmitInfo(vk::CommandBuffer& cmd, SemaphoreList& waitingPoints,
		SemaphoreList& signalList, PipelineStageList& pipelineStages) const;

	// cloning of resources
	AQUA_API virtual Node* Clone(vkLib::Context ctx) const;
	AQUA_API void CloneDependencies(vkLib::Context ctx, const Node* src);

	// getters
	GraphTraversalState GetTraversalState() const { return TraversalState; }
	const std::vector<Dependency>& GetInputConnections() const { return InputConnections; }
	const std::vector<Dependency>& GetOutputConnections() const { return OutputConnections; }

	Node() : NodeId(-1) {}

	Node(NodeID nodeId)
		: NodeId(nodeId) { }

	virtual ~Node() = default;
};

using NodeRef = SharedRef<Node>;

template <typename _Pipeline>
bool UpdateResource(const _Pipeline& pipeline, const GraphRsc& rsc);

inline Node* Clone(vkLib::Context ctx, const Node* op) { return op->Clone(ctx); }

EXEC_END
AQUA_END

template <typename _Pipeline>
bool AQUA_NAMESPACE::EXEC_NAMESPACE::UpdateResource(const _Pipeline& pipeline, const GraphRsc& resource)
{
	switch (resource.Type)
	{
		case vk::DescriptorType::eSampledImage:
			UpdateSampledImage(pipeline, resource.Location, resource.ImageView, resource.Sampler);
			break;
		case vk::DescriptorType::eStorageBuffer:
			UpdateStorageBuffer(pipeline, resource.Location, resource.Buffer);
			break;
		case vk::DescriptorType::eUniformBuffer:
			UpdateUniformBuffer(pipeline, resource.Location, resource.Buffer);
			break;
		case vk::DescriptorType::eStorageImage:
			UpdateStorageImage(pipeline, resource.Location, resource.ImageView);
			break;
		default:
			break;
	}

	return true;
}

