#include "Core/Aqpch.h"
#include "Execution/Graph.h"

void AQUA_NAMESPACE::EXEC_NAMESPACE::SerializeExecutionWavefronts(vkLib::Context ctx, const std::vector<Graph>& graphs, const std::vector<Wavefront>& inputLayers)
{
	for (size_t i = 0; i < graphs.size() - 1; i++)
	{
		const auto& leadingOps = graphs[i].OutputNodes;
		const auto& followingOps = inputLayers[i];

		for (const auto& leadOpID : leadingOps)
		{
			for (const auto& followOpID : followingOps)
			{
				auto semaphore = ctx.CreateSemaphore();

				DependencyInjection inInj;
				inInj.SetCurrOp(followOpID);
				inInj.Connect(leadOpID);
				inInj.SetSignal(semaphore);
				inInj.SetWaitPoint(vk::PipelineStageFlagBits::eTopOfPipe);

				_STL_VERIFY(graphs[i].InjectOutputDependencies(inInj), "Couldn't serialize the execution graph");

				DependencyInjection outInj;
				outInj.SetCurrOp(leadOpID);
				outInj.Connect(followOpID);
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

AQUA_NAMESPACE::EXEC_NAMESPACE::Graph AQUA_NAMESPACE::EXEC_NAMESPACE::Clone(vkLib::Context ctx, const Graph& graph)
{
	Graph cloned = graph;

	cloned.Lock = MakeRef<std::mutex>();

	// todo: here we clone all the nodes while respecting the dependencies

	for (auto& [name, node] : cloned.Nodes)
	{
		*node = Clone(ctx, *node);
	}

	// cloning dependencies
	for (auto& [name, node] : cloned.Nodes)
	{
		// set the input and output dependency nodes
		for (auto& dependency : node->InputConnections)
		{
			dependency.From = dependency.From;
			dependency.To = node->NodeId;
		}

		for (auto& dependency : node->OutputConnections)
		{
			dependency.From = node->NodeId;
			dependency.To = dependency.To;
		}
	}

	return cloned;
}
