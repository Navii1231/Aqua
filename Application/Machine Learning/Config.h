#pragma once
#include "Execution/Graph.h"
#include "Execution/Draft.h"

// the returned id is the index as classified by the feature
// the input is the id of the sample as classified by the dataset
using Index = Aqua::Exec::NodeID;
using _FloatType = float;

enum class ClassType
{
	eInvalid            = 0,
	eClassification     = 1,
	eNumeric            = 2,
};

// features are essentially lambdas and therefore 
// they can access the data set on their own
using FeatureFn = std::function<Index(Index)>;
using NumericFn = std::function<_FloatType(Index)>;
using Bucket = std::vector<Index>;
using Classification = std::map<Index, Bucket>;
using Heuristic = std::function<float(const Classification&)>;

template<typename ..._Types>
using Table = std::map<Index, std::tuple<_Types...>>;

Index GetSize(const Classification& cft)
{
	Index size = 0;

	for (const auto& [idx, bin] : cft)
	{
		size += bin.size();
	}

	return size;
}

float GiniImpurity(const Classification& cft)
{
	auto count = GetSize(cft);
	float impurity = 1.0f;

	for (const auto& [idx, buc] : cft)
	{
		float prob = buc.size() / (float)count;
		impurity -= prob * prob;
	}

	return impurity;
}

float InfoLoss(const Classification& cft)
{
	auto count = GetSize(cft);
	float loss = 0.0f;

	for (const auto& [idx, buc] : cft)
	{
		float prob = buc.size() / (float)count;

		float currEntropy = -prob * std::log(prob);

		if (prob < std::numeric_limits<float>::epsilon())
			currEntropy = prob;

		loss += currEntropy;
	}

	return loss;
}

// the feature used to classify discrete or continuous values
struct Feature
{
	FeatureFn FeatFn;
	NumericFn NumFn;
	Heuristic HeurFn = GiniImpurity;

	ClassType GetFeatureClassification() const
	{
		if (FeatFn)
			return ClassType::eClassification;
		else if (NumFn)
			return ClassType::eNumeric;

		return ClassType::eInvalid;
	}
};

Classification ClassifyAgainstPredicate(const FeatureFn& feat, const Bucket& buc)
{
	Classification cft{};

	for (auto id : buc)
	{
		cft[feat(id)].emplace_back(id);
	}

	return cft;
}

_FloatType CalcEntropy(const Classification& cft, const Heuristic& heur, const FeatureFn& pred)
{
	_FloatType ent = 0.0f;

	for (const auto& [idx, buc] : cft)
	{
		ent += heur(ClassifyAgainstPredicate(pred, buc)) * buc.size() / GetSize(cft);
	}

	return ent;
}

// class, entropy, splitVal (for continuous classification)
std::tuple<Classification, _FloatType, _FloatType> Classify(const Feature& feat, const FeatureFn& pred, const Bucket& buc)
{
	if (feat.GetFeatureClassification() == ClassType::eClassification)
	{
		Classification  cft = ClassifyAgainstPredicate(feat.FeatFn, buc);
		return { cft, CalcEntropy(cft, feat.HeurFn, pred), std::numeric_limits<float>::quiet_NaN() };
	}

	Classification minCft{};
	_FloatType minEnt = std::numeric_limits<_FloatType>::max();
	_FloatType minSplitVal = std::numeric_limits<_FloatType>::quiet_NaN();

	// either greater than or less than characterized by 1, 0 indices
	for (auto splitIdx : buc)
	{
		auto splitVal = feat.NumFn(splitIdx);
		Classification cft{};

		for (auto testIdx : buc)
		{
			cft[feat.NumFn(testIdx) >= splitVal].emplace_back(testIdx);
		}

		_FloatType ent = 0.0f;

		for (const auto& [idx, buc] : cft)
		{
			ent += feat.HeurFn(::ClassifyAgainstPredicate(pred, buc)) * buc.size() / GetSize(cft);
		}

		if (ent < minEnt)
		{
			minEnt = ent;
			minCft = cft;
			minSplitVal = splitVal;
		}
	}

	return { minCft, minEnt, minSplitVal };
}

using FeatureBin = std::map<Index, Feature>;

struct DecisionNode
{
	Index Idx{};
	Feature Feat{};
	float Entropy = std::numeric_limits<float>::max();
	float Split = std::numeric_limits<float>::max(); // for regression trees

	Classification Cft{};

	std::map<Index, Aqua::SharedRef<DecisionNode>> Children;

	ClassType GetClassificationType() const { return Feat.GetFeatureClassification(); }
};
