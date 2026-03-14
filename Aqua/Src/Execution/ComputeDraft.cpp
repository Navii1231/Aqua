#include "Core/Aqpch.h"
#include "Execution/ComputeDraft.h"
#include "Execution/Parser.h"
#include "Execution/CodeGenerator.h"

#include "Utils/CompilerErrorChecker.h"

std::expected<AQUA_NAMESPACE::EXEC_NAMESPACE::Graph, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::ComputeDraft::Construct(const std::vector<NodeID>& probes, int threadCount) const
{
	std::map<NodeID, KernelExtractions> allExts{};

	Graph graph{};
	graph.OutputNodes = probes;

	auto front = _ConstructEx<NodeRef>(probes, true, [this, &allExts, &graph](NodeID node, const std::string& kernel)->NodeRef
		{
			if (graph.Nodes.find(node) != graph.Nodes.end())
				return graph.Nodes[node];

			// this is where we shall create our compute node
			KernelExtractions exts = GLSLParser(kernel, 440).Extract();
			allExts[node] = exts;

			SharedRef<ComputeNode> computeNode = MakeRef<ComputeNode>(node, mCtx.MakePipelineBuilder().BuildComputePipeline<vkLib::ComputePipeline>(ConstructShader(exts)));

			auto success = InsertResources(*computeNode, exts);

			graph.Nodes[node] = computeNode;

			return computeNode;
		}, [this, &graph](const NodeInfo& from, const NodeInfo& to, vk::PipelineStageFlags stageFlags)
			{
				Dependency dependency{};
				dependency.SetIncomingOP(from.ID);
				dependency.SetOutgoingOP(to.ID);
				dependency.SetSignal(mCtx.CreateSemaphore());
				dependency.SetWaitPoint(stageFlags);

				graph.Nodes[from.ID]->AddOutputConnection(dependency);
				graph.Nodes[to.ID]->AddInputConnection(dependency);
			}, threadCount);

	if (!front)
		return std::unexpected(front.error());

	graph.InputNodes = *front;

	return graph;
}

vkLib::PShader AQUA_NAMESPACE::EXEC_NAMESPACE::ComputeDraft::ConstructShader(const KernelExtractions& extraction) const
{
	GLSLCodeGenerator generator(extraction);
	std::string code = generator.Generate();

	vkLib::PShader shader{};

	shader.SetShader("eCompute", code);

	// read from the kernel extraction...
	shader.AddMacro("LOCAL_SIZE_X", std::to_string(extraction.WorkGroupSize.x));
	shader.AddMacro("LOCAL_SIZE_Y", std::to_string(extraction.WorkGroupSize.y));
	shader.AddMacro("LOCAL_SIZE_Z", std::to_string(extraction.WorkGroupSize.z));

	for (const auto& [type, str] : sNumericTypesToStrings)
	{
		if (type == TypeName::eInvalid || type == TypeName::eVoid || type == TypeName::eBoolean)
			continue;

		shader.AddMacro(str, *ConvertToGLSLString(type));
	}

	auto results = shader.CompileShaders();

	CompileErrorChecker checker("");
	checker.AssertOnError(results);

	return shader;
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::ComputeDraft::InsertResources(ComputeNode& node, const KernelExtractions& exts) const
{
	for (const auto& [location, rsc] : exts.Rscs)
	{
		auto rscRef = MakeRef(rsc);
		node.Resources[vkLib::ConvertIntoMapKey(rsc.Location)] = rscRef;
	}

	for (const auto& pushConsts : exts.KernelConsts)
	{
		node.KernelConsts[pushConsts.Idx] = pushConsts;
	}

	return true;
}
