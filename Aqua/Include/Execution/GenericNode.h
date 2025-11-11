#pragma once
#include "CBScope.h"
#include "Node.h"

AQUA_BEGIN
EXEC_BEGIN

using OpFn = std::function<void(vk::CommandBuffer, const GenericNode*)>;
using OpUpdateFn = std::function<void(GenericNode*)>;

struct GenericNode : public Node
{
	SharedRef<vkLib::ComputePipeline> Cmp;
	SharedRef<vkLib::GraphicsPipeline> GFX;

	OpType Type = OpType::eNone;

	OpFn Fn = [](vk::CommandBuffer, const GenericNode*) {};
	OpUpdateFn UpdateFn = [](GenericNode*) {}; // could be used to update descriptors

	std::uintptr_t OpID = 0;

	void SetOpFn(OpFn&& fn) { Fn = fn; }

	// execution
	inline virtual std::expected<const vkLib::BasicPipeline*, OpType> GetBasicPipeline() const override;
	inline virtual std::expected<vkLib::BasicPipeline*, OpType> GetBasicPipeline() override;
	inline virtual bool Execute(vk::CommandBuffer cmd) const override;
	inline virtual bool Update() override;

	// cloning override
	inline virtual Node* Clone(vkLib::Context ctx) const override;

	OpType GetOpType() const { return Type; }

	// constructors
	GenericNode() = default;
	GenericNode(NodeID id) : Node(id) {}

	GenericNode(NodeID nodeId, OpType type)
		: Node(nodeId), Type(type) { }

	virtual ~GenericNode() = default;
};

EXEC_END
AQUA_END

std::expected<const vkLib::BasicPipeline*, AQUA_NAMESPACE::EXEC_NAMESPACE::OpType>
AQUA_NAMESPACE::EXEC_NAMESPACE::GenericNode::GetBasicPipeline() const
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

std::expected<vkLib::BasicPipeline*, AQUA_NAMESPACE::EXEC_NAMESPACE::OpType> AQUA_NAMESPACE::EXEC_NAMESPACE::GenericNode::GetBasicPipeline()
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

bool AQUA_NAMESPACE::EXEC_NAMESPACE::GenericNode::Execute(vk::CommandBuffer cmd) const
{
	Fn(cmd, this);
	return true;
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::GenericNode::Update()
{
	UpdateFn(this);
	return true;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Node* AQUA_NAMESPACE::EXEC_NAMESPACE::GenericNode::Clone(vkLib::Context ctx) const
{
	GenericNode* cloned = new GenericNode(*this);

	cloned->GFX = Aqua::SharedRef<vkLib::GraphicsPipeline>(Aqua::Clone(ctx, Aqua::GetRefAddr(GFX)));
	cloned->Cmp = Aqua::SharedRef<vkLib::ComputePipeline>(Aqua::Clone(ctx, Aqua::GetRefAddr(Cmp)));

	// cloning dependencies
	cloned->CloneDependencies(ctx, this);

	return cloned;
}

