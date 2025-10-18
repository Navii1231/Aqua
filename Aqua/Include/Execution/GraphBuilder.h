#pragma once
#include "GraphConfig.h"
#include "CBScope.h"
#include "Graph.h"

AQUA_BEGIN
EXEC_BEGIN

class GraphBuilder
{
public:
	GraphBuilder() = default;
	GraphBuilder(vkLib::Context ctx) : mCtx(ctx) {}

	~GraphBuilder() = default;

	void SetCtx(vkLib::Context ctx) { mCtx = ctx; }

	AQUA_API void Clear();
	void ClearOperations() { mOperations.clear(); }
	void ClearDependencies() { mInputDependencies.clear(); }

	// internal dependencies
	AQUA_API void InsertDependency(const std::string& from, const std::string& to, vk::PipelineStageFlags stageFlags = vk::PipelineStageFlagBits::eTopOfPipe);

	template <typename Pipeline>
	void InsertPipelineOp(const std::string& name, const Pipeline& pipeline);

	// Redundant operations that do no contribute to the final outcome are excluded
	// absolutely convergent on the probe node
	AQUA_API std::expected<Graph, GraphError> GenerateExecutionGraph(const vk::ArrayProxy<std::string>& pathEnds) const;
	// Build the execution graph and sorts the entries out according to their execution order
	Operation& operator[](const std::string& name) { return mOperations[name]; }
	const Operation& operator[](const std::string& name) const { return mOperations.at(name); }

private:
	std::unordered_map<std::string, Operation> mOperations;

	// From "To" nodes --> Dependencies
	mutable std::unordered_map<std::string, std::vector<DependencyMetaData>> mInputDependencies;

	vkLib::Context mCtx;

private:
	std::expected<bool, GraphError> BuildDependencySkeleton(std::unordered_map<std::string, SharedRef<Operation>>& opCache, const std::string& name, const vk::ArrayProxy<std::string>& probes, Wavefront& inputs) const;
	// After the graph is validated, we create and emplace the semaphores between dependent operations
	void EmplaceSemaphore(Dependency& connection) const;

	std::expected<bool, GraphError> ValidateDependencyInputs() const;
	SharedRef<Operation> CreateOperation(const std::string& name) const;
};

template <typename Pipeline>
void AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::InsertPipelineOp(const std::string& name, const Pipeline& pipeline)
{
	switch (pipeline.GetPipelineBindPoint())
	{
		case vk::PipelineBindPoint::eGraphics:
			mOperations[name].States.Type = OpType::eGraphics;
			mOperations[name].GFX = std::reinterpret_pointer_cast<vkLib::GraphicsPipeline>(MakeRef<Pipeline>(pipeline));
			break;
		case vk::PipelineBindPoint::eCompute:
			mOperations[name].States.Type = OpType::eCompute;
			mOperations[name].Cmp = std::reinterpret_pointer_cast<vkLib::ComputePipeline>(MakeRef<Pipeline>(pipeline));
			break;
		case vk::PipelineBindPoint::eRayTracingKHR:
			mOperations[name].States.Type = OpType::eRayTracing;
			_STL_ASSERT(false, "Ray tracing pipeline is yet to implement in the vkLib");
			break;
		default:
			return;
	}
}

void SerializeExecutionWavefronts(GraphBuilder& builder, const std::vector<Wavefront>& layers);

EXEC_END
AQUA_END
