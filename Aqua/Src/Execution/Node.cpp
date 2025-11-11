#include "Core/Aqpch.h"
#include "Execution/Node.h"

void AQUA_NAMESPACE::EXEC_NAMESPACE::Node::operator()(vk::CommandBuffer cmds, vkLib::Core::Worker executor) const
{
	ExecState = State::eExecute;
	Execute(cmds);

	SemaphoreList waitingList, signalList;
	PipelineStageList pipelineStages;

	vk::SubmitInfo submitInfo = SetupSubmitInfo(cmds, waitingList, signalList, pipelineStages);
	executor.Enqueue(submitInfo);

	ExecState = State::eReady;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Node::operator()(vk::CommandBuffer cmds,
	vkLib::Core::Ref<vkLib::Core::WorkerQueue> worker, vk::Fence fence) const
{
	ExecState = State::eExecute;
	Execute(cmds);

	SemaphoreList waitingList, signalList;
	PipelineStageList pipelineStages;

	vk::SubmitInfo submitInfo = SetupSubmitInfo(cmds, waitingList, signalList, pipelineStages);
	worker->Submit(submitInfo, fence);

	ExecState = State::eReady;
}

vk::SubmitInfo AQUA_NAMESPACE::EXEC_NAMESPACE::Node::SetupSubmitInfo(vk::CommandBuffer& cmd,
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

AQUA_API AQUA_NAMESPACE::EXEC_NAMESPACE::Node* AQUA_NAMESPACE::EXEC_NAMESPACE::Node::Clone(vkLib::Context ctx) const
{
	Node* cloned = new Node(*this);

	// cloning dependencies
	cloned->CloneDependencies(ctx, this);

	return cloned;
}

AQUA_API void AQUA_NAMESPACE::EXEC_NAMESPACE::Node::CloneDependencies(vkLib::Context ctx, const Node* src)
{
	InputConnections = src->InputConnections;
	OutputConnections = src->OutputConnections;
	InputInjections = src->InputInjections;
	OutputInjections = src->OutputInjections;

	for (auto& dependency : InputConnections)
	{
		dependency.SetSignal(ctx.CreateSemaphore());
	}

	for (auto& dependency : OutputConnections)
	{
		dependency.SetSignal(ctx.CreateSemaphore());
	}

	for (auto& dependency : InputInjections)
	{
		dependency.SetSignal(ctx.CreateSemaphore());
	}

	for (auto& dependency : OutputInjections)
	{
		dependency.SetSignal(ctx.CreateSemaphore());
	}
}
