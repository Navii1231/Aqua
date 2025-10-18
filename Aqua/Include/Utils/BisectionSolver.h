#pragma once
#include "../Core/AqCore.h"

AQUA_BEGIN

template <typename Sc>
using BisectionFunction = std::function<Sc(Sc)>;

template <typename Sc>
class BisectionSolver
{
public:
	using MyScalar = Sc;
	using MyFunction = BisectionFunction<Sc>;

public:
	BisectionSolver() = default;
	~BisectionSolver() = default;

	void SetFunc(MyFunction&& fn) { mFn = fn; }
	void SetTolerence(Sc sc) { mTol = sc; }

	Sc Solve(Sc first, Sc second) const;

private:
	MyFunction mFn;
	Sc mTol = Sc(0.01);

private:
	bool CheckSameSigns(Sc first, Sc second) const
		{ return mFn(first) * mFn(second) > Sc(0.0); }
};

template <typename Sc>
Sc AQUA_NAMESPACE::BisectionSolver<Sc>::Solve(Sc first, Sc second) const
{
	_STL_ASSERT(!CheckSameSigns(first, second), "The two endpoints give the same sign");

	Sc root = (first + second) / Sc(2.0);

	while (mFn(root) * mFn(root) > mTol * mTol)
	{
		if (CheckSameSigns(first, root))
			second = root;
		else
			first = root;

		root = (first + second) / Sc(2.0);
	}

	return root;
}

using fBisectionSolver = BisectionSolver<float>;
using dBisectionSolver = BisectionSolver<double>;

AQUA_END
