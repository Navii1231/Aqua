#pragma once
#include "Graph.h"

AQUA_BEGIN
EXEC_BEGIN

enum class EnsembleState
{
	eValid = 0,
	eIntermediate = 1,
	eInvalid = 2,
};

// may use variadic templates to allow multiple types of graphs
template <typename _Graph>
class BasicEnsemble
{
public:
	using MyGraph = _Graph;
	using MyEnsemble = BasicEnsemble<MyGraph>;
	using GraphSeq = std::vector<MyGraph>;
	using EnsembleSeq = std::vector<MyEnsemble>;
	using SeqVariant = std::variant<GraphSeq, EnsembleSeq>;

public:
	BasicEnsemble() = default;

	// core API
	void Update() const;
	typename MyGraph::Executable SortEntries() const;
	std::vector<typename MyGraph::Executable> SortEntriesByGroups() const;

	GraphSeq GetGraphs() const { return std::get<GraphSeq>(mVariant); }
	EnsembleSeq GetEnsembleSeq() const { return std::get<EnsembleSeq>(mVariant); }

	const BasicEnsemble& operator[](size_t idx) const { return std::get<EnsembleSeq>(mVariant)[idx]; }
	BasicEnsemble& operator[](size_t idx) { return std::get<EnsembleSeq>(mVariant)[idx]; }

	typename EnsembleSeq::iterator begin() { return std::get<EnsembleSeq>(mVariant).begin(); }
	typename EnsembleSeq::const_iterator begin() const { return std::get<EnsembleSeq>(mVariant).begin(); }
	typename EnsembleSeq::iterator end() { return std::get<EnsembleSeq>(mVariant).end(); }
	typename EnsembleSeq::const_iterator end() const { return std::get<EnsembleSeq>(mVariant).end(); }

	const MyGraph& Fetch(size_t idx) const { return std::get<GraphSeq>(mVariant)[idx]; }
	MyGraph& Fetch(size_t idx) { return std::get<GraphSeq>(mVariant)[idx]; }

	EnsembleState GetState() const { return mState; }

	Wavefront GetInputWavefront() const;
	Wavefront GetOutputWavefront() const;

	void SetCtx(vkLib::Context ctx) { mCtx = ctx; }
	void SetSeq(const SeqVariant& seq) { mVariant = seq; }

	// checking out the ensemble content
	bool IsGraphSeq() const { return std::holds_alternative<GraphSeq>(mVariant); }
	bool IsEnsemble() const { return std::holds_alternative<EnsembleSeq>(mVariant); }

	// creates the nodes
	static BasicEnsemble MakeSeq(vkLib::Context ctx, const GraphSeq& seq);
	static BasicEnsemble MakeSeq(vkLib::Context ctx, const EnsembleSeq& seq);

	template <typename It>
	static BasicEnsemble MakeSeq(vkLib::Context ctx, It begin, It end);

	static BasicEnsemble Flatten(const BasicEnsemble& ensemble);
	static BasicEnsemble Heapify(const BasicEnsemble& flatEnsemble, const std::vector<size_t>& cuts);

	template <typename _Ensemble, typename GraphPred, typename EnsPred>
	static TraversalState Traverse(_Ensemble&& ensemble, GraphPred&& graphPred, EnsPred&& ensPred);

	static void UpdateEnsemble(const BasicEnsemble& ensemble);
	static void SortEnsembleEntries(MyGraph::Executable& entries, const BasicEnsemble& ensemble);

private:
	vkLib::Context mCtx;
	SeqVariant mVariant;

	mutable EnsembleState mState = EnsembleState::eInvalid;

private:
	void SetState(EnsembleState state) const;
};

using Ensemble = BasicEnsemble<Graph>;

template <typename _Graph>
template <typename It>
BasicEnsemble<_Graph> BasicEnsemble<_Graph>::MakeSeq(vkLib::Context ctx, It begin, It end)
{
	return MakeSeq(ctx, std::vector(begin, end));
}

template <typename _Graph>
template <typename _Ensemble, typename GraphPred, typename EnsPred>
TraversalState BasicEnsemble<_Graph>::Traverse(_Ensemble&& ensemble, GraphPred&& graphPred, EnsPred&& ensPred)
{
	auto state = ensPred(std::forward<_Ensemble>(ensemble));

	if (state != TraversalState::eSuccess)
		return state;

	if (ensemble.IsGraphSeq())
	{
		auto& graphs = std::get<Ensemble::GraphSeq>(ensemble.mVariant);

		for (auto&& graph : graphs)
		{
			auto state = graphPred(graph);

			if (state != TraversalState::eSuccess)
				return state;
		}

		return TraversalState::eSuccess;
	}

	auto& ensembles = std::get<Ensemble::EnsembleSeq>(ensemble.mVariant);

	for (auto&& ensemble : ensembles)
	{
		auto state = Ensemble::Traverse(std::forward<_Ensemble>(ensemble),
			std::forward<GraphPred>(graphPred), std::forward<EnsPred>(ensPred));

		if (state == TraversalState::eQuit)
			return state;
	}

	return TraversalState::eSuccess;
}

template <typename _Graph>
void BasicEnsemble<_Graph>::Update() const
{
	UpdateEnsemble(*this);
}

template <typename _Graph>
typename BasicEnsemble<_Graph>::MyGraph::Executable BasicEnsemble<_Graph>::SortEntries() const
{
	Graph::Executable entries;
	SortEnsembleEntries(entries, *this);

	return entries;
}

template <typename _Graph>
std::vector<typename BasicEnsemble<_Graph>::MyGraph::Executable> BasicEnsemble<_Graph>::SortEntriesByGroups() const
{
	std::vector<Graph::Executable> entryGroups;

	Traverse(*this, [](const Graph& graph)
		{
			return TraversalState::eSuccess;
		}, [&entryGroups](const BasicEnsemble& ensemble)
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

template <typename _Graph>
Wavefront BasicEnsemble<_Graph>::GetInputWavefront() const
{
	Wavefront inputWavefront;

	Traverse(*this, [&inputWavefront](const Graph& graph)
		{
			inputWavefront = graph.InputNodes;

			return TraversalState::eQuit; // quit once the input is found
		}, [](const BasicEnsemble&) { return TraversalState::eSuccess; });

	return inputWavefront;
}

template <typename _Graph>
Wavefront BasicEnsemble<_Graph>::GetOutputWavefront() const
{
	Wavefront outWavefront;

	Traverse(*this, [&outWavefront](const Graph& graph)
		{
			outWavefront = graph.InputNodes;
			return TraversalState::eSuccess;
		}, [](const BasicEnsemble&) { return TraversalState::eSuccess; });

	return outWavefront;
}

template <typename _Graph>
BasicEnsemble<_Graph> BasicEnsemble<_Graph>::MakeSeq(vkLib::Context ctx, const GraphSeq& seq)
{
	BasicEnsemble ensemble;
	ensemble.mState = seq.empty() ? EnsembleState::eIntermediate : EnsembleState::eValid;

	Aqua::Exec::SerializeExecutionWavefronts(ctx, seq);

	ensemble.SetCtx(ctx);
	ensemble.SetSeq(seq);

	return ensemble;
}

template <typename _Graph>
BasicEnsemble<_Graph> BasicEnsemble<_Graph>::MakeSeq(vkLib::Context ctx, const EnsembleSeq& seq)
{
	BasicEnsemble ensemble;

	ensemble.SetCtx(ctx);
	ensemble.SetSeq(seq);

	return ensemble;
}

template <typename _Graph>
BasicEnsemble<_Graph> BasicEnsemble<_Graph>::Flatten(const BasicEnsemble& ensemble)
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

	BasicEnsemble flattened{};
	flattened.mState = EnsembleState::eValid;
	flattened.SetSeq(mergedGraphs);

	return flattened;
}

template <typename _Graph>
BasicEnsemble<_Graph> BasicEnsemble<_Graph>::Heapify(const BasicEnsemble& flatEnsemble, const std::vector<size_t>& cuts)
{
	// Must be flat
	if (!flatEnsemble.IsGraphSeq())
		throw std::runtime_error("Heapify expects a flattened leaf ensemble.");

	if (flatEnsemble.mState != EnsembleState::eValid)
		throw std::runtime_error("Can't heapify an intermediate/invalid ensemble");

	const auto& flatSeq = flatEnsemble.GetGraphs();
	std::vector<BasicEnsemble::GraphSeq> regions;
	regions.reserve(cuts.size() + 1);

	size_t start = 0;
	for (size_t cut : cuts)
	{
		if (cut > flatSeq.size())
			throw std::out_of_range("Cut index exceeds flat sequence size.");

		BasicEnsemble::GraphSeq region(flatSeq.begin() + start, flatSeq.begin() + cut);
		regions.push_back(std::move(region));
		start = cut;

	}

	// Add remaining graphs after the last cut
	if (start < flatSeq.size())
	{
		BasicEnsemble::GraphSeq region(flatSeq.begin() + start, flatSeq.end());
		regions.push_back(std::move(region));
	}

	// Convert each region into a leaf ensemble
	BasicEnsemble::EnsembleSeq children;
	children.reserve(regions.size());

	for (auto& region : regions)
	{
		// The boundaries now should have no injections
		region.front().ClearInputInjections();
		region.back().ClearOutputInjections();

		children.push_back(BasicEnsemble::MakeSeq(flatEnsemble.mCtx, region));
	}

	// Form new intermediate ensemble (depth incremented)
	flatEnsemble.mState = EnsembleState::eInvalid;

	BasicEnsemble root;
	root.mCtx = flatEnsemble.mCtx;
	root.mVariant = children;
	root.mState = EnsembleState::eValid;

	return root;
}

template <typename _Graph>
void BasicEnsemble<_Graph>::UpdateEnsemble(const BasicEnsemble& ensemble)
{
	BasicEnsemble::Traverse(ensemble, [](const Graph& graph)
		{
			graph.Update();
			return TraversalState::eSuccess;
		}, [](const BasicEnsemble& ensemble) { return TraversalState::eSuccess; });
}

template <typename _Graph>
void BasicEnsemble<_Graph>::SortEnsembleEntries(MyGraph::Executable& entries, const BasicEnsemble& ensemble)
{
	BasicEnsemble::Traverse(ensemble, [&entries](const Graph& graph)
		{
			entries.append_range(graph.SortEntries());
			return TraversalState::eSuccess;
		}, [](const BasicEnsemble& ensemble) { return TraversalState::eSuccess; });
}

template <typename _Graph>
void BasicEnsemble<_Graph>::SetState(EnsembleState state) const
{
	BasicEnsemble::Traverse(*this, [](const Graph& graph) {return TraversalState::eSuccess; },
		[state](const BasicEnsemble& ensemble)
		{
			ensemble.mState = state;
			return TraversalState::eSuccess;
		});
}

// other related functions
template <typename _Graph>
BasicEnsemble<_Graph> Clone(vkLib::Context ctx, const BasicEnsemble<_Graph>& ensemble)
{
	BasicEnsemble cloned = ensemble;

	BasicEnsemble::Traverse(cloned, [&ctx](Graph& graph)
		{
			graph = Clone(ctx, graph);
			return TraversalState::eSuccess;
		}, [&ctx](BasicEnsemble& ensemble)
			{
				return TraversalState::eSuccess;
			});

		return cloned;
}

EXEC_END
AQUA_END
