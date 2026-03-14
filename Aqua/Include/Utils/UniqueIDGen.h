#pragma once
#include "Random.h"

AQUA_BEGIN

template <typename _Int, typename _Engine = Aqua::MersenneTwister<_Int>>
class BasicUniqueIDGen
{
public:
	using MyInt = _Int;
	using MyEngine = _Engine;
	using MyRNG = Random<_Int, std::uniform_int_distribution<_Int>, _Engine>;

public:
	BasicUniqueIDGen() = default;
	BasicUniqueIDGen(MyInt min, MyInt max)
		: mRNG(min, max) { }

	~BasicUniqueIDGen() = default;

	MyInt Generate() const
	{
		while (true)
		{
			auto rdm = mRNG();

			if (mDuplicates.find(rdm) == mDuplicates.end())
			{
				mDuplicates.insert(rdm);
				return rdm;
			}
		}

		return -1;
	}

	void InsertDuplicate(MyInt num) { mDuplicates.insert(num); }

	void Flush() { mDuplicates.clear(); }

	MyInt operator()() const { return Generate(); }

private:
	mutable MyRNG mRNG;
	mutable std::set<MyInt> mDuplicates;
};

AQUA_END
