#pragma once
#include "../Core/AqCore.h"

AQUA_BEGIN

template <typename T>
using MersenneTwister = std::conditional_t<sizeof(T) <= 4, std::mt19937, std::mt19937_64>;

template <typename T, typename Distribution, typename Engine = MersenneTwister<T>>
class Random
{
public:
	using MyType = T;
	using MyDist = Distribution;
	using MyEng = Engine;

public:
	Random() : mEngine(std::random_device()()), mDistribution() {}

	Random(const Random& random) : mEngine(random.mEngine), mDistribution(random.mDistribution) {}

	template <typename _FirstArg, typename ..._Args>
	Random(const _FirstArg& firstArg, _Args&&... args)
		: mEngine(std::random_device()()), mDistribution(firstArg, std::forward<_Args>(args)...) {}

	T Generate() { return GenerateFromDist(mDistribution); }

	template <typename ..._Args>
	T Generate(_Args&&... args)
	{
		Distribution customDist(std::forward<_Args>(args)...);
		return GenerateFromDist(customDist);
	}

	T GenerateFromDist(Distribution& dist) { return dist(mEngine); }

	T operator()() { return Generate(); }
	
	template <typename ..._Args>
	T operator()(_Args&&... args) { return Generate(std::forward<_Args>(args)...); }

	Distribution GetDistribution() const { return mDistribution; }

private:
	Engine mEngine;
	Distribution mDistribution;
};

// integers
template <std::integral _Int>
using UniformInts = Random<_Int, std::uniform_int_distribution<_Int>>;

template <std::integral _Int>
using BinomialInts = Random<_Int, std::binomial_distribution<_Int>>;

template <std::integral _Int>
using PoissonInts = Random<_Int, std::poisson_distribution<_Int>>;

// floating point values
template <std::floating_point _Float>
using UniformFloats = Random<_Float, std::uniform_real_distribution<_Float>>;

template <std::floating_point _Float>
using ChiSquaredFloats = Random<_Float, std::chi_squared_distribution<_Float>>;

template <std::floating_point _Float>
using LogNormalFloats = Random<_Float, std::lognormal_distribution<_Float>>;

template <std::floating_point _Float>
using NormalFloats = Random<_Float, std::normal_distribution<_Float>>;

AQUA_END
