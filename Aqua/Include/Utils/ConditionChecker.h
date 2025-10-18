#pragma once
#include "../Core/AqCore.h"

AQUA_BEGIN

class ConditionChecker
{
public:
	ConditionChecker(bool checkInDest = true)
		: mCheckInDestructor(checkInDest) {}

	void SetMsg(const std::string& msg) { mMessage = msg; }
	void SetBoolean(bool& var) { mConditionRef = &var; }

	~ConditionChecker()
	{
		_STL_ASSERT(mConditionRef, mMessage);
	}

private:
	bool* mConditionRef = nullptr;
	bool mCheckInDestructor = true;

	std::string mMessage;
};

AQUA_END
