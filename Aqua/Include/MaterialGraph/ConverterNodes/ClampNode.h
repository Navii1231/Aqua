#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

enum class ClampMode 
{
	eMinMax,
	eMinOnly,
	eMaxOnly
};

class ClampNode : public MaterialNode
{
public:
	ClampNode() {
		mChildNodes["Value"].AddValue("Float", "0.0");
		mChildNodes["Min"].AddValue("Float", "0.0");
		mChildNodes["Max"].AddValue("Float", "1.0");
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		std::stringstream code;

		const std::string& Val = values[0];
		const std::string& Min = values[1];
		const std::string& Max = values[2];

		switch (mClampMode) {
			case ClampMode::eMinMax: code << "clamp(" << Val << ", " << Min << ", " << Max << ")"; break;
			case ClampMode::eMinOnly: code << "max(" << Val << ", " << Min << ")"; break;
			case ClampMode::eMaxOnly: code << "min(" << Val << ", " << Max << ")"; break;
			default: code << Val;
		}

		return code.str();
	}

	ClampMode mClampMode = ClampMode::eMinMax;
};

AQUA_END
