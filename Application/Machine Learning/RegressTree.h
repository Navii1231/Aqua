#pragma once
#include "config.h"

// you've to take care of the uneven interval here
// the predicate can't really manage the uneven intervals
// or make the loss function a custom function
template <typename Float>
class RegressTree
{
public:
	using FloatType = Float;
	using Predicate = std::function<Float(Index)>;

	struct Range
	{
		Index Split;
		FloatType Val{};
		FloatType Loss{};

		std::vector<Aqua::SharedRef<Range>> Children;
	};

public:
	RegressTree() = default;
	~RegressTree() = default;

	void SetRange(Index begin, Index end) { mBegin = begin; mEnd = end; }
	void SetTolerance(FloatType tolerance) { mTolerance = tolerance; }
	void SetPredicate(const Predicate& pred) { mPredicate = pred; }

	Aqua::SharedRef<Range> Construct()
	{
		return Split(mBegin, mEnd);
	}

private:
	Bucket mIndices;
	Index mBegin, mEnd;
	Predicate mPredicate;
	FloatType mTolerance = 0.01f;

private:
	Float Loss(Index begin, Index end, Float mean) const
	{
		if (begin == end)
			return 0.0f;

		Float loss = 0.0f;

		for (; begin < end; begin++)
		{
			auto diff = mPredicate(begin) - mean;
			loss += diff * diff;
		}

		return loss;
	}

	Float Mean(Index begin, Index end) const
	{
		if (begin == end)
			return 0.0f;

		Float mean = 0.0f;
		Index count = end - begin;

		for (; begin < end; begin++)
		{
			mean += mPredicate(begin);
		}

		return mean / count;
	}

	Aqua::SharedRef<Range> Split(Index begin, Index end)
	{
		Aqua::SharedRef<Range> node = Aqua::MakeRef<Range>();
		node->Split = end + (end - begin) / 2;
		node->Val = Mean(begin, end);
		node->Loss = Loss(begin, end, node->Val);

		if (node->Loss < mTolerance)
			return node;

		for (Index curr = begin; curr < end; curr++)
		{
			auto loss = Loss(begin, curr, Mean(begin, curr)) + Loss(curr, end, Mean(curr, end));

			if (loss < node->Loss)
			{
				node->Split = curr;
				node->Loss = loss;
			}
		}

		node->Children.emplace_back(Split(begin, node->Split));
		node->Children.emplace_back(Split(node->Split, end));

		return node;
	}
};

using fRegresTree = RegressTree<float>;
using dRegresTree = RegressTree<double>;
