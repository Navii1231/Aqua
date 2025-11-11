#pragma once
#include "Application.h"
#include "Execution/Draft.h"
#include "Execution/ComputeNode.h"

enum class NeuralNodeType
{
	eNone             = 0,
	eLayer            = 1,
	eActivation       = 2,
	eFilter           = 3,
	eCostFn           = 4,
};

struct NeuralNodeInfo
{
	int LayerCount = 0;
	std::string ActivationCode;
	glm::uvec2 FilterSize = { 0, 0 };

	std::string CostFunctionCode;

	NeuralNodeType NodeType;
};

// basis of neural network
class NeuralDraftAttempt : public Aqua::Exec::Draft<std::string, std::string>
{
public:
	using NodeInfo = typename MyDraftType::NodeInfo<Aqua::Exec::NodeRef>;

public:
	NeuralDraftAttempt(vkLib::Context ctx) : mCtx(ctx) {}

	void SetCostFunction(const std::string& func);

	std::expected<Aqua::Exec::Graph, Aqua::Exec::GraphError> ConstructForward(const Aqua::Exec::Wavefront& probes);

	std::expected<Aqua::Exec::Graph, Aqua::Exec::GraphError> ConstructBackward(const Aqua::Exec::Wavefront& probes);

private:
	vkLib::Context mCtx;
	Aqua::Exec::NodeID mIdx = 0;

	// the executable is generated through two passes
	// first once constructs a graph of meta infos
	// second one optimizes the meta info network and 
	// generates the actual executable graph
	Aqua::Exec::Draft<NeuralNodeInfo> mSecondPass;
};
