#include "Core/Aqpch.h"
#include "Execution/Ensemble.h"

void AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::Update() const
{
	UpdateEnsemble(*this);
}

AQUA_NAMESPACE::EXEC_NAMESPACE::GraphList AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::SortEntries() const
{
	GraphList entries;
	SortEnsembleEntries(entries, *this);

	return entries;
}

std::vector<AQUA_NAMESPACE::EXEC_NAMESPACE::GraphList> AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::SortEntriesByGroups() const
{
	std::vector<GraphList> entryGroups;

	Traverse(*this, [](const Graph& graph)
		{
			return TraversalState::eSuccess;
		}, [&entryGroups](const Ensemble& ensemble)
			{
				if (!ensemble.IsGraphSeq())
					return TraversalState::eSuccess;

				if (entryGroups.size() >= entryGroups.capacity())
					entryGroups.reserve(2 * entryGroups.size());

				entryGroups.emplace_back(ensemble.SortEntries());
				return TraversalState::eSuccess;
			});

		entryGroups.shrink_to_fit();

		return entryGroups;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Wavefront AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::GetInputWavefront() const
{
	Wavefront inputWavefront;

	Traverse(*this, [&inputWavefront](const Graph& graph)
		{
			inputWavefront = graph.InputNodes;

			return TraversalState::eQuit; // quit once the input is found
		}, [](const Ensemble&) { return TraversalState::eSuccess; });

	return inputWavefront;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Wavefront AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::GetOutputWavefront() const
{
	Wavefront outWavefront;

	Traverse(*this, [&outWavefront](const Graph& graph)
		{
			outWavefront = graph.InputNodes;
			return TraversalState::eSuccess;
		}, [](const Ensemble&) { return TraversalState::eSuccess; });

	return outWavefront;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::MakeSeq(vkLib::Context ctx, const GraphSeq& seq)
{
	Ensemble ensemble;
	ensemble.mState = seq.empty() ? EnsembleState::eIntermediate : EnsembleState::eValid;

	Aqua::Exec::SerializeExecutionWavefronts(ctx, seq);

	ensemble.SetCtx(ctx);
	ensemble.SetSeq(seq);

	return ensemble;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::MakeSeq(vkLib::Context ctx, const EnsembleSeq& seq)
{
	Ensemble ensemble;

	ensemble.SetCtx(ctx);
	ensemble.SetSeq(seq);

	return ensemble;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::Flatten(const Ensemble& ensemble)
{
	GraphSeq mergedGraphs;

	if (ensemble.mState != EnsembleState::eValid)
		throw std::runtime_error("Can't flatten an intermediate/invalid ensemble");

	if (ensemble.IsGraphSeq())
		return ensemble;
	else if (ensemble.IsEnsemble())
	{
		// recursively flatten child ensembles
		for (const auto& child : ensemble.GetEnsembleSeq())
		{
			uint32_t splitIdx = static_cast<uint32_t>(mergedGraphs.size());
			const auto joinedChild = Flatten(child);
			child.mState = EnsembleState::eInvalid;
			mergedGraphs.append_range(joinedChild.GetGraphs());

			if (splitIdx != 0)
			{
				// forming new semaphore barriers
				SerializeExecutionWavefronts(ensemble.mCtx, { mergedGraphs[splitIdx - 1], mergedGraphs[splitIdx] });
			}
		}
	}

	ensemble.mState = EnsembleState::eIntermediate;

	Ensemble flattened{};
	flattened.mState = EnsembleState::eValid;
	flattened.SetSeq(mergedGraphs);

	return flattened;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::Heapify(const Ensemble& flatEnsemble, const std::vector<size_t>& cuts)
{
	// Must be flat
	if (!flatEnsemble.IsGraphSeq())
		throw std::runtime_error("Heapify expects a flattened leaf ensemble.");

	if (flatEnsemble.mState != EnsembleState::eValid)
		throw std::runtime_error("Can't heapify an intermediate/invalid ensemble");

	const auto& flatSeq = flatEnsemble.GetGraphs();
	std::vector<Ensemble::GraphSeq> regions;
	regions.reserve(cuts.size() + 1);

	size_t start = 0;
	for (size_t cut : cuts)
	{
		if (cut > flatSeq.size())
			throw std::out_of_range("Cut index exceeds flat sequence size.");

		Ensemble::GraphSeq region(flatSeq.begin() + start, flatSeq.begin() + cut);
		regions.push_back(std::move(region));
		start = cut;

	}

	// Add remaining graphs after the last cut
	if (start < flatSeq.size())
	{
		Ensemble::GraphSeq region(flatSeq.begin() + start, flatSeq.end());
		regions.push_back(std::move(region));
	}

	// Convert each region into a leaf ensemble
	Ensemble::EnsembleSeq children;
	children.reserve(regions.size());

	for (auto& region : regions)
	{
		// The boundaries now should have no injections
		region.front().ClearInputInjections();
		region.back().ClearOutputInjections();

		children.push_back(Ensemble::MakeSeq(flatEnsemble.mCtx, region));
	}

	// Form new intermediate ensemble (depth incremented)
	flatEnsemble.mState = EnsembleState::eInvalid;

	Ensemble root;
	root.mCtx = flatEnsemble.mCtx;
	root.mVariant = children;
	root.mState = EnsembleState::eValid;

	return root;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::UpdateEnsemble(const Ensemble& ensemble)
{
	Ensemble::Traverse(ensemble, [](const Graph& graph)
		{
			graph.Update();
			return TraversalState::eSuccess;
		}, [](const Ensemble& ensemble) { return TraversalState::eSuccess; });
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::SortEnsembleEntries(GraphList& entries, const Ensemble& ensemble)
{
	Ensemble::Traverse(ensemble, [&entries](const Graph& graph)
		{
			entries.append_range(graph.SortEntries());
			return TraversalState::eSuccess;
		}, [](const Ensemble& ensemble) { return TraversalState::eSuccess; });
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::SetState(EnsembleState state) const
{
	Ensemble::Traverse(*this, [](const Graph& graph) {return TraversalState::eSuccess; },
		[state](const Ensemble& ensemble)
		{
			ensemble.mState = state;
			return TraversalState::eSuccess;
		});
}

// other functions
AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Clone(vkLib::Context ctx, const Ensemble& ensemble)
{
	Ensemble cloned = ensemble;

	Ensemble::Traverse(cloned, [&ctx](Graph& graph)
		{
			graph = Clone(ctx, graph);
			return TraversalState::eSuccess;
		}, [&ctx](Ensemble& ensemble)
			{
				return TraversalState::eSuccess;
			});

		return cloned;
}
