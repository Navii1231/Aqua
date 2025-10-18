#include "Core/Aqpch.h"
#include "Execution/GraphBuilder.h"
#include "Execution/Graph.h"

void AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::Clear()
{
	ClearDependencies();
	ClearOperations();
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::InsertDependency(const std::string& from,
	const std::string& to, vk::PipelineStageFlags stageFlags)
{
	mInputDependencies[to].push_back({ from, to, stageFlags });
}

std::expected<AQUA_NAMESPACE::EXEC_NAMESPACE::Graph, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError>
	AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::GenerateExecutionGraph(
	const vk::ArrayProxy<std::string>& pathEnds) const
{
	for (const auto& path : pathEnds)
	{
		if (mOperations.find(path) == mOperations.end())
			return std::unexpected(GraphError::ePathDoesntExist);
	}
	auto error = ValidateDependencyInputs();

	if (!error)
		return std::unexpected(error.error());

	std::unordered_map<std::string, SharedRef<Operation>> operations;
	Wavefront inputs;

	for (const auto& path : pathEnds)
	{
		auto error = BuildDependencySkeleton(operations, path, pathEnds, inputs);

		if (!error)
			return std::unexpected(error.error());
	}

	Graph graph;
	graph.InputNodes = inputs;
	graph.OutputNodes = std::vector<std::string>(pathEnds.begin(), pathEnds.end());
	graph.Nodes = operations;
	graph.Lock = MakeRef<std::mutex>();
	graph.Ctx = mCtx;

	auto validated = graph.Validate();

	if (!validated)
		return std::unexpected(validated.error());

	return graph;
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::BuildDependencySkeleton(std::unordered_map<std::string, SharedRef<Operation>>& opCache, const std::string& name, const vk::ArrayProxy<std::string>& probes, Wavefront& inputs) const
{
	//if (std::find(probes.begin(), probes.end(), name) != probes.end())
	//	return std::unexpected(GraphError::ePathReferencedMoreThanOnce);

	if (opCache.find(name) != opCache.end())
		return true;

	opCache[name] = CreateOperation(name);
	opCache[name]->Name = name;

	if (mInputDependencies.at(name).empty())
		inputs.push_back(name);

	for (const auto& dependencyData : mInputDependencies.at(name))
	{
		auto error = BuildDependencySkeleton(opCache, dependencyData.From, probes, inputs);

		if (!error)
			return std::unexpected(error.error());

		Dependency dependency{};
		dependency.SetOutgoingOP(opCache[name]);
		dependency.SetWaitPoint(dependencyData.WaitingStage);
		dependency.SetIncomingOP(opCache[dependencyData.From]);

		EmplaceSemaphore(dependency);

		opCache[name]->AddInputConnection(dependency);
		opCache[dependencyData.From]->AddOutputConnection(dependency);
	}

	return true;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::EmplaceSemaphore(Dependency& connection) const
{
	connection.SetSignal(mCtx.CreateSemaphore());
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::ValidateDependencyInputs() const
{
	for (const auto& [name, op] : mOperations)
	{
		auto& list = mInputDependencies[name];

		for (auto& dependency : list)
		{
			if (name == dependency.From)
				return std::unexpected(GraphError::eDependencyUponItself);

			if (mOperations.find(dependency.From) == mOperations.end())
				return std::unexpected(GraphError::eInvalidConnection);
		}
	}

	for (const auto& [name, dependency] : mInputDependencies)
	{
		if (mOperations.find(name) == mOperations.end())
			return std::unexpected(GraphError::eInvalidConnection);
	}

	return true;
}

AQUA_NAMESPACE::SharedRef<AQUA_NAMESPACE::EXEC_NAMESPACE::Operation> AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::CreateOperation(const std::string& name) const
{
	auto opPtr = MakeRef<Operation>(mOperations.at(name));
	opPtr->States.TraversalState = GraphTraversalState::ePending;

	return opPtr;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::SerializeExecutionWavefronts(GraphBuilder& builder, const std::vector<Wavefront>& layers)
{
	// each consecutive operation in a wavefront is linked with the every other operation in its surrounding wavefronts
	// if the wavefront doesn't exist, we will skip that particular wavefront and insert the next/previous in its place

	// here we're removing all the empty wavefronts
	std::vector<Wavefront> nonEmptyWavefronts{};
	nonEmptyWavefronts.reserve(layers.size());

	for (const auto& layer : layers)
	{
		if (layer.empty())
			continue;

		nonEmptyWavefronts.emplace_back(layer);
	}

	// no linking if they're all empty
	if (nonEmptyWavefronts.empty())
		return;

	for (size_t i = 1; i < nonEmptyWavefronts.size(); i++)
	{
		const auto& leadingOps = nonEmptyWavefronts[i - 1];
		const auto& followingOps = nonEmptyWavefronts[i];

		for (const auto& leadingOpName : leadingOps)
		{
			for (const auto& followingOpName : followingOps)
			{
				builder.InsertDependency(leadingOpName, followingOpName);
			}
		}
	}
}
