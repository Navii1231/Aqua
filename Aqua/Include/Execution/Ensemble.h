#pragma once
#include "Graph.h"

AQUA_BEGIN
EXEC_BEGIN

enum class TraversalState
{
	eSuccess = 0,
	eQuit = 1,
	eSkip = 2,
};

enum class EnsembleState
{
	eValid = 0,
	eIntermediate = 1,
	eInvalid = 2,
};

class Ensemble
{
public:
	using GraphSeq = std::vector<Graph>;
	using EnsembleSeq = std::vector<Ensemble>;
	using SeqVariant = std::variant<GraphSeq, EnsembleSeq>;

public:
	Ensemble() = default;

	// core API
	AQUA_API void Update() const;
	AQUA_API GraphList SortEntries() const;
	AQUA_API std::vector<GraphList> SortEntriesByGroups() const;

	GraphSeq GetGraphs() const { return std::get<GraphSeq>(mVariant); }
	EnsembleSeq GetEnsembleSeq() const { return std::get<EnsembleSeq>(mVariant); }

	const Ensemble& operator[](size_t idx) const { return std::get<EnsembleSeq>(mVariant)[idx]; }
	Ensemble& operator[](size_t idx) { return std::get<EnsembleSeq>(mVariant)[idx]; }

	typename EnsembleSeq::iterator begin() { return std::get<EnsembleSeq>(mVariant).begin(); }
	typename EnsembleSeq::const_iterator begin() const { return std::get<EnsembleSeq>(mVariant).begin(); }
	typename EnsembleSeq::iterator end() { return std::get<EnsembleSeq>(mVariant).end(); }
	typename EnsembleSeq::const_iterator end() const { return std::get<EnsembleSeq>(mVariant).end(); }

	const Graph& Fetch(size_t idx) const { return std::get<GraphSeq>(mVariant)[idx]; }
	Graph& Fetch(size_t idx) { return std::get<GraphSeq>(mVariant)[idx]; }

	EnsembleState GetState() const { return mState; }

	AQUA_API Wavefront GetInputWavefront() const;
	AQUA_API Wavefront GetOutputWavefront() const;

	void SetCtx(vkLib::Context ctx) { mCtx = ctx; }
	void SetSeq(const SeqVariant& seq) { mVariant = seq; }

	// checking out the ensemble content
	bool IsGraphSeq() const { return std::holds_alternative<GraphSeq>(mVariant); }
	bool IsEnsemble() const { return std::holds_alternative<EnsembleSeq>(mVariant); }

	// creates the nodes
	AQUA_API static Ensemble MakeSeq(vkLib::Context ctx, const GraphSeq& seq);
	AQUA_API static Ensemble MakeSeq(vkLib::Context ctx, const EnsembleSeq& seq);

	template <typename It>
	static Ensemble MakeSeq(vkLib::Context ctx, It begin, It end);

	AQUA_API static Ensemble Flatten(const Ensemble& ensemble);
	AQUA_API static Ensemble Heapify(const Ensemble& flatEnsemble, const std::vector<size_t>& cuts);

	template <typename _Ensemble, typename GraphPred, typename EnsPred>
	static TraversalState Traverse(_Ensemble&& ensemble, GraphPred&& graphPred, EnsPred&& ensPred);

	AQUA_API static void UpdateEnsemble(const Ensemble& ensemble);
	AQUA_API static void SortEnsembleEntries(GraphList& entries, const Ensemble& ensemble);

private:
	vkLib::Context mCtx;
	SeqVariant mVariant;

	mutable EnsembleState mState = EnsembleState::eInvalid;

private:
	void SetState(EnsembleState state) const;
};

template <typename It>
Ensemble AQUA_NAMESPACE::EXEC_NAMESPACE::Ensemble::MakeSeq(vkLib::Context ctx, It begin, It end)
{
	return MakeSeq(ctx, std::vector(begin, end));
}

template <typename _Ensemble, typename GraphPred, typename EnsPred>
TraversalState Ensemble::Traverse(_Ensemble&& ensemble, GraphPred&& graphPred, EnsPred&& ensPred)
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

// other related functions
AQUA_API Ensemble Clone(vkLib::Context ctx, const Ensemble& ensemble);

EXEC_END
AQUA_END
