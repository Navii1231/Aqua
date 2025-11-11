#pragma once
#include "Execution/Graph.h"

class Layer
{
public:
	Layer() = default;
	Layer(bool deferStart)
		: mStartInvoked(!deferStart) {}

	virtual ~Layer() = default;

	virtual bool OnStart() = 0;
	virtual bool OnUpdate(std::chrono::nanoseconds) = 0;

	bool HasStartInvoked() const { return mStartInvoked; }

private:
	bool mStartInvoked = false;
};
