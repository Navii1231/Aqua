#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

enum class MixMode 
{
	eMix,
	eAdd,
	eMultiply,
	eScreen,
	eSubtract
};

class MixNode : public MaterialNode
{
public:
	MixNode() 
	{
		mChildNodes["Color1"].AddValue("Vec3", "0.0");
		mChildNodes["Color2"].AddValue("Vec3", "0.0");
		mChildNodes["Factor"].AddValue("Float", "0.5");
	}

	std::string EvaluateMix(const std::string& a, const std::string& b, const std::string& f) const 
	{
		switch (mMode) {
			case MixMode::eMix: return "mix(" + a + ", " + b + ", " + f + ")";
			case MixMode::eAdd: return a + " + " + b;
			case MixMode::eMultiply: return a + " * " + b;
			case MixMode::eScreen: return "1.0 - (1.0 - " + a + ") * (1.0 - " + b + ")";
			case MixMode::eSubtract: return a + " - " + b;
			default: return "vec3(0.0)";
		}
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		return EvaluateMix(values[0], values[1], values[2]) << ";";
	}

	MixMode mMode = MixMode::eMix;
};

AQUA_END

