#pragma once
#include "AutoDiffConfig.h"

// a semi red black tree... red corresponds to addition, black corresponds to multiplication
// not thread safe...
template <typename _NodeT>
class AutoDiff
{
public:
	using MyNodeID = Aqua::Exec::NodeID;
	using MyNodeT = _NodeT;
	using MyNodeRefT = Aqua::SharedRef<MyNodeT>;
	using MyGraph = Aqua::Exec::BasicGraph<MyNodeRefT>;
	// first parm: id of the node you wanna take derivative of
	// second parm: idx of the child node wrt the derivative node
	using MyDeriFn = std::function<MyGraph(MyNodeID, MyNodeID)>;
	using MyNodeTraversalStates = typename MyGraph::MyNodeTraversalStates;
	using MyIndexMap = std::map<MyNodeID, MyNodeID>;
	using MyRNGFn = std::function<MyNodeID()>;

public:
	AutoDiff() : mRNG(Aqua::Exec::UniqueIDGen()) {}
	AutoDiff(MyRNGFn rng) : mRNG(rng) {}
	~AutoDiff() = default;

	void SetInputVariables(const Aqua::Exec::Wavefront& inWave) { mInputVariables = inWave; }
	void SetIDGen(const MyRNGFn& rng) { mRNG = rng; }
	void SetDerivFn(const MyDeriFn& fn) { mDerivativeFn = fn; }
	void SetExpr(const MyGraph& expr) { mExpr = expr; }

	MyGraph Apply()
	{
		MyGraph finalExpr{};
		MyIndexMap graphToResult;
		MyNodeTraversalStates traversalStates;

		for (auto& [ID, node] : mExpr.Nodes)
			traversalStates[ID] = Aqua::Exec::GraphTraversalState::ePending;

		Aqua::Exec::Traverse(mExpr, [this, &finalExpr, &traversalStates, &graphToResult](Aqua::Exec::NodeID id)
			{
				if (traversalStates[id] == Aqua::Exec::GraphTraversalState::eVisited)
					return Aqua::Exec::TraversalState::eSkip;

				if (std::find(mInputVariables.begin(), mInputVariables.end(), id) != mInputVariables.end())
				{
					// we can prune out some paths which
					// don't lead to a differential parameters
					// that's one optimization...

					// TODO: should be handled by GetDerivative method
					auto [rID, state] = Aqua::Exec::CloneEx(finalExpr.Nodes, mExpr.Nodes, id, [this, &finalExpr, id, &traversalStates, &graphToResult](Aqua::Exec::NodeID nodeId)
						{
							auto rID = GetRandom();
							finalExpr.Nodes[rID] = Aqua::MakeRef(*mExpr.Nodes[nodeId]);
							finalExpr.Nodes[rID]->SetNodeID(rID);
			
							graphToResult[nodeId] = rID;
							traversalStates[id] = Aqua::Exec::GraphTraversalState::eVisited;

							return std::make_pair(rID, Aqua::Exec::TraversalState::eSuccess);

						}, [](MyNodeRefT ref)->Aqua::Exec::Wavefront& { return GetChildren(*ref); });

					// set the differential
					finalExpr.Nodes[rID]->SetCalType(CalcNodeType::eDifferential);
					finalExpr.InputNodes.push_back(rID);

					return Aqua::Exec::TraversalState::eSkip;
				}

				return Aqua::Exec::TraversalState::eSuccess;
			}, [this, &finalExpr, &graphToResult, &traversalStates](Aqua::Exec::NodeID id, const Aqua::Exec::Wavefront& children)
				{
					MyNodeRefT plusNode = ConstructOp(CalcNodeType::eAccumulation);

					for (MyNodeID i = 0; i < children.size(); i++)
					{
						// we can collapse the constant multiplications branches
						// that's the second optimization...
						auto diff = finalExpr.Nodes[graphToResult[children[i]]];

						if (!diff)
							continue;

						auto mulID = GetRandom();

						MyNodeRefT mulNode = ConstructOp(CalcNodeType::eExpansion);
						mulNode->SetNodeID(mulID);

						auto deriv = GetDerivative(id, i);
						_STL_ASSERT(deriv.OutputNodes.size() == 1, "Invalid input node size");

						GetChildren(*mulNode).push_back(*deriv.OutputNodes.begin());
						GetChildren(*mulNode).push_back(diff->mNodeID);

						GetChildren(*plusNode).push_back(mulID);

						finalExpr.Nodes[mulID] = mulNode;

						for (const auto& [ID, node] : deriv.Nodes)
						{
							finalExpr.Nodes[ID] = node;
						}
					}

					traversalStates[id] = Aqua::Exec::GraphTraversalState::eVisited;

					if (GetChildren(*plusNode).empty())
						return Aqua::Exec::TraversalState::eSkip;

					auto plusID = GetRandom();
					plusNode->mNodeID = plusID;
					graphToResult[id] = plusID;

					finalExpr.Nodes[plusID] = plusNode;
					
					return Aqua::Exec::TraversalState::eSuccess;
				}, [](MyNodeRefT ref) { return GetChildren(*ref); });

		for (Aqua::Exec::NodeID i = 0; i < mExpr.OutputNodes.size(); i++)
		{
			finalExpr.OutputNodes.push_back(graphToResult[mExpr.OutputNodes[i]]);
		}

		return finalExpr;
	}

private:
	MyGraph mExpr;

	Aqua::Exec::Wavefront mInputVariables;
	MyDeriFn mDerivativeFn;

	MyRNGFn mRNG;

private:
	static MyNodeRefT ConstructOp(CalcNodeType type)
	{
		auto node = Aqua::MakeRef<MyNodeT>();
		node->mCalType = type;
		return node;
	}

	MyGraph GetDerivative(Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)
	{ return mDerivativeFn(nodeID, childID); }

	Aqua::Exec::NodeID GetRandom() const { return mRNG(); }
};
