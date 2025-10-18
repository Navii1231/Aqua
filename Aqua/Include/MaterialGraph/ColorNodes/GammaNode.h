#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

enum class GammaSpace 
{
	eLinearToSRGB,
	eSRGBToLinear
};

class GammaNode : public MaterialNode
{
public:
	GammaNode() 
	{
		mChildNodes["Color"].AddValue("Vec3", "0.0");
	}

	std::string EvaluateGamma() const 
	{
		switch (mGammaMode) {
			case GammaSpace::eLinearToSRGB: return "pow(Color, vec3(1.0/2.2))";
			case GammaSpace::eSRGBToLinear: return "pow(Color, vec3(2.2))";
			default: return "Color";
		}
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		std::stringstream code;

		const std::string& Color = values[0];

		switch (mGammaMode) {
			case GammaSpace::eLinearToSRGB: code << "pow(" << Color << ", vec3(1.0/2.2))"; break;
			case GammaSpace::eSRGBToLinear: code << "pow(" << Color << ", vec3(2.2))"; break;
			default: code << Color;
		}

		return code.str();
	}

	GammaSpace mGammaMode = GammaSpace::eLinearToSRGB;
};

AQUA_END
