#pragma once
#include "config.h"
#include "IndexTable.h"

// now we gotta translate all this code into GPU our execution model
// random forests may consist of on average around 100s of trees
// it makes sense to parallelize all these operations
class DecisionTree
{
public:
	DecisionTree() = default;
	~DecisionTree() = default;

	void SetPredicate(FeatureFn pred) { mPredicate = pred; }
	void SetDataSet(const Bucket& buc) { mIndices = buc; }
	void SetTolerance(_FloatType tol) { mTolerance = tol; }
	void SetMaxDepth(Index depth) { mMaxDepth = depth; }

	auto Classify(Index idx) const { return ::Classify(mFeatures.at(idx), mPredicate, mIndices); }

	Aqua::SharedRef<DecisionNode> Construct(Index relativeTo = -1)
	{
		std::set<Index> remainingFeats{};

		for (const auto& [idx, feature] : mFeatures)
			remainingFeats.insert(idx);

		Aqua::SharedRef<DecisionNode> root = ClassifyPurestFeature(remainingFeats, mIndices);

		TryErasingFeature(remainingFeats, root, 0);
		ConstructTree(root, remainingFeats, 1);

		return root;
	}

	void SetFeatureBin(Index idx, const FeatureFn& fn, const Heuristic& heur = GiniImpurity)
	{ mFeatures[idx] = { fn, {}, heur }; }
	void SetFeatureBin(Index idx, const NumericFn& fn, const Heuristic& heur = GiniImpurity)
	{ mFeatures[idx] = { {}, fn, heur }; }

	Feature& operator[](Index idx) { return mFeatures[idx]; }
	const Feature& operator[](Index idx) const { return mFeatures.at(idx); }

private:
	Bucket mIndices;
	FeatureBin mFeatures;

	_FloatType mTolerance = 0.01f;
	Index mMaxDepth = std::numeric_limits<Index>::max();

	FeatureFn mPredicate{};

private:
	bool TryErasingFeature(std::set<Index>& remainingFeats, Aqua::SharedRef<DecisionNode> node, Index depth)
	{
		if (mFeatures[node->Idx].GetFeatureClassification() == ClassType::eNumeric && !CheckStoppingCondition(node, depth))
			return false;

		remainingFeats.erase(node->Idx);
		return true;
	}
	
	bool CheckStoppingCondition(Aqua::SharedRef<DecisionNode> node, Index depth)
	{
		if (node->Entropy < mTolerance || depth > mMaxDepth)
			return true;

		return false;
	}

private:
	Aqua::SharedRef<DecisionNode> ClassifyPurestFeature(const std::set<Index>& feats, const Bucket& buc)
	{
		if (feats.empty() || buc.empty())
			return Aqua::SharedRef<DecisionNode>();

		Aqua::SharedRef<DecisionNode> node = Aqua::MakeRef<DecisionNode>();

		for (auto idx : feats)
		{
			// only works for discrete values
			// what if the feature consists of a numeric
			// function instead of a classification
			const auto& feature = mFeatures[idx];
			auto [cft, entropy, splitVal] = ::Classify(feature, mPredicate, buc);

			if (entropy < node->Entropy)
			{
				node->Entropy = entropy;
				node->Idx = idx;
				node->Cft = cft;
				node->Feat = feature; // optional

				if (feature.GetFeatureClassification() == ClassType::eNumeric)
					node->Split = splitVal;
			}
		}

		return node;
	}

	void ConstructTree(Aqua::SharedRef<DecisionNode> node, std::set<Index>& remainingFeats, Index depth)
	{
		if (remainingFeats.empty() || CheckStoppingCondition(node, depth))
			return;

		for (const auto& [idx, buc] : node->Cft)
		{
			if (buc.empty())
				continue;

			node->Children[idx] = ClassifyPurestFeature(remainingFeats, buc);
		}

		TryErasingFeature(remainingFeats, node, depth);

		for (const auto& [idx, child] : node->Children)
		{
			ConstructTree(child, remainingFeats, depth + 1);
		}
	}
};
