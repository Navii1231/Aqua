#pragma once
#include "config.h"
#include "Utils/Random.h"
#include "../Dependencies/Include/Eigen/Eigen"

class LogisticRegress
{
public:
	using MyFloat = _FloatType;
	using MyVec = Eigen::VectorX<_FloatType>;
	using MyMat = Eigen::MatrixX<_FloatType>;

public:
	LogisticRegress() = default;
	~LogisticRegress() = default;

	void SetTemp(MyFloat temp) { mTemp = temp; }
	MyVec Forward(Index idx) const { return SoftMax(mWeights * PrepareInput(idx) + mBiases, mTemp); }
	void SetData(const Bucket& indices) { mIndices = indices; }

	void SetPredicate(const FeatureFn& fn) { mPredicate = fn; }

	Feature& operator[](Index idx) { return mFeatures[idx]; }
	const Feature& operator[](Index idx) const { return mFeatures.at(idx); }

	void Init(MyFloat mean, MyFloat sigma)
	{
		auto [probDist, idxMap] = GetProbDist();

		mProbDist = probDist;
		mIndexMap = idxMap;

		InitializeWeights(mean, sigma, mFeatures.size(), probDist.rows());
	}

	std::tuple<MyMat, MyVec> BackPropagate(Index idx, MyFloat spd)
	{
		// todo: creating new vectors and matrix with each iteration
		// inefficient for now, but we'll fix it later
		auto ins = PrepareInput(idx);
		auto est = SoftMax(mWeights * ins + mBiases, mTemp);
		auto gradWeight = CalcWeightDerivative(mProbDist, est, ins);
		auto gradBias = CalcLossDerivative(mProbDist, est);

		return { gradWeight, gradBias };
	}

	void ExecuteEpoch(MyFloat spd)
	{
		MyMat weightAccum(mWeights.rows(), mWeights.cols());
		MyVec biasAccum(mBiases.rows());

		weightAccum.setZero();
		biasAccum.setZero();

		for (auto idx : mIndices)
		{
			auto [gradW, gradB] = BackPropagate(idx, spd);

			weightAccum += gradW;
			biasAccum += gradB;
		}

		weightAccum *= spd / mIndices.size();
		biasAccum *= spd / mIndices.size();

		mWeights -= weightAccum;
		mBiases -= biasAccum;
	}

	std::tuple<MyVec, std::map<Index, Index>> GetProbDist() const
	{
		auto cft = ::ClassifyAgainstPredicate(mPredicate, mIndices);

		MyVec probDist(cft.size());
		std::map<Index, Index> idxMap{};

		Index probIdx = 0;
		Index size = GetSize(cft);

		for (const auto& [idx, buc] : cft)
		{
			probDist[probIdx] = buc.size() / (MyFloat)size;
			idxMap[probIdx++] = idx;
		}

		return { probDist, idxMap };
	}

	MyMat CalcWeightDerivative(const MyVec& probDist, const MyVec& estProbs, const MyVec& inputs)
	{
		MyMat matrix(probDist.rows(), inputs.rows());

		for (Index i = 0; i < (Index) matrix.rows(); i++)
		{
			for (Index j = 0; j < (Index) matrix.cols(); j++)
			{
				matrix(i, j) = (probDist[i] - estProbs[i]) * inputs[j];
			}
		}

		return matrix;
	}

	MyVec CalcLossDerivative(const MyVec& probDist, const MyVec& estimatedProbs)
	{
		return probDist - estimatedProbs;
	}

	// training with gradient descent, powered by the chain rule

private:
	MyMat mWeights;
	MyVec mBiases;

	Bucket mIndices;
	FeatureBin mFeatures;

	FeatureFn mPredicate{};

	MyVec mProbDist{};
	std::map<Index, Index> mIndexMap{};

	MyFloat mTemp = 1.0f;

private:
	MyVec PrepareInput(Index whichOne) const
	{
		MyVec embedding(mFeatures.size());

		for (Index i = 0; i < mFeatures.size(); i++)
		{
			embedding[i] = mFeatures.at(i).NumFn(whichOne);
		}

		return embedding;
	}

	MyVec SoftMax(const MyVec& vec, MyFloat temperature = 1.0f) const
	{
		MyVec result(vec.rows());
		MyFloat normalization = 0.0f;

		for (Index i = 0; i < (Index) vec.rows(); i++)
		{
			result[i] = glm::exp(-temperature * vec[i]);
			normalization += result[i];
		}

		return result / normalization;
	}

	void InitializeWeights(MyFloat mean, MyFloat sigma, Index x, Index y)
	{
		Aqua::NormalFloats<MyFloat> normalDist(mean, sigma);

		mWeights = MyMat(x, y);
		mBiases = MyVec(y);

		for (Index j = 0; j < y; j++)
		{
			for (Index i = 0; i < x; i++)
			{
				mWeights(i, j) = normalDist.Generate();
			}

			mBiases[j] = normalDist.Generate();
		}
	}
};
