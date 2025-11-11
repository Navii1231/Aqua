#pragma once
#include "CBScope.h"
#include "Utils.h"
#include "Node.h"
#include "ParserConfig.h"

AQUA_BEGIN
EXEC_BEGIN

struct ComputeNode : public Node
{
	SharedRef<vkLib::ComputePipeline> Cmp;

	ResourceDescMap Resources;
	std::map<uint32_t, PushConstDecl> KernelConsts;

	glm::uvec3 InvocationCount = { 1, 1, 1 };

	std::expected<const vkLib::BasicPipeline*, OpType> GetBasicPipeline() const override { return GetRefAddr(Cmp); }
	std::expected<vkLib::BasicPipeline*, OpType> GetBasicPipeline() override { return GetRefAddr(Cmp); }
	inline bool Execute(vk::CommandBuffer cmd) const override;
	inline bool Update() override;

	inline Node* Clone(vkLib::Context ctx) const override;

	GraphRsc& operator()(uint32_t set, uint32_t binding) { return *Resources[vkLib::ConvertIntoMapKey({ set, binding, 0 })]; }
	const GraphRsc& operator()(uint32_t set, uint32_t binding) const { return *Resources.at(vkLib::ConvertIntoMapKey({ set, binding, 0 })); }

	template <typename T>
	void SetKernelConst(uint32_t idx, const T& constant);

	ComputeNode(NodeID nodeId) : Node(nodeId) {}
	inline ComputeNode(NodeID nodeId, const vkLib::ComputePipeline& pipeline);
};

template <typename T>
inline void SetComputeRsc(Node& node, uint32_t set, uint32_t binding, vkLib::Buffer<T> buffer)
{
	ConvertNode<ComputeNode>(node)(set, binding).Buffer = buffer;
}

inline void SetComputeRsc(Node& node, uint32_t set, uint32_t binding, vkLib::ImageView view, vkLib::Core::Ref<vk::Sampler> sampler)
{
	ConvertNode<ComputeNode>(node)(set, binding).ImageView = view;
	ConvertNode<ComputeNode>(node)(set, binding).Sampler = sampler;
}

template <typename T>
inline void SetKernelConst(Node& node, uint32_t idx, const T& constant)
{
	ConvertNode<ComputeNode>(node).SetKernelConst(idx, constant);
}

EXEC_END
AQUA_END

bool AQUA_NAMESPACE::EXEC_NAMESPACE::ComputeNode::Execute(vk::CommandBuffer buffer) const
{
	CBScope scope(buffer);

	auto& pipeline = *GetRefAddr(Cmp);

	pipeline.Begin(buffer);
	pipeline.Activate();

	for (const auto& [idx, kernelConst] : KernelConsts)
	{
		std::stringstream constName;
		constName << "eCompute." << kernelConst.sConstName << ".Index_" << kernelConst.Idx;
		PushConst(pipeline, constName.str(), kernelConst.Values.size(), kernelConst.Values.data());
	}

	pipeline.Dispatch(InvocationCount);
	pipeline.End();

	return true;
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::ComputeNode::Update()
{
	bool success = true;

	for (const auto& [location, rsc] : Resources)
	{
		success &= UpdateResource(*Cmp, *rsc);

		if (!success)
			return false;
	}

	return success;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Node* AQUA_NAMESPACE::EXEC_NAMESPACE::ComputeNode::Clone(vkLib::Context ctx) const
{
	ComputeNode* cloned = new ComputeNode(*this);

	cloned->Cmp = Aqua::SharedRef<vkLib::ComputePipeline>(Aqua::Clone(ctx, Aqua::GetRefAddr(Cmp)));

	// cloning dependencies
	cloned->CloneDependencies(ctx, this);

	return cloned;
}

template <typename T>
void AQUA_NAMESPACE::EXEC_NAMESPACE::ComputeNode::SetKernelConst(uint32_t idx, const T& constant)
{
	if (KernelConsts.find(idx) == KernelConsts.end())
		return;

	auto& values = KernelConsts[idx].Values;

	values.resize(sizeof(constant)); // make sure there's enough space for the data
	std::memcpy(values.data(), &constant, sizeof(constant));
}

AQUA_NAMESPACE::EXEC_NAMESPACE::ComputeNode::ComputeNode(NodeID nodeId, const vkLib::ComputePipeline& pipeline)
	: Node(nodeId)
{
	Cmp = MakeRef(pipeline);
}
