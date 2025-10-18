#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

enum class FalloffType 
{
	eQuadratic,
	eLinear,
	eConstant
};

class LightFalloffNode : public MaterialNode 
{
public:
	LightFalloffNode() 
	{
		mChildNodes["Distance"].AddValue("Float", "1.0");
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		std::stringstream code;

		const std::string& Distance = values[0];

		switch (mFalloffType)
		{
			case FalloffType::eQuadratic: code << "1.0 / (" << Distance << " * " << Distance << ")"; break;
			case FalloffType::eLinear: code << "1.0 / " << Distance; break;
			case FalloffType::eConstant: code << "1.0"; break;
			default: code << "0.0";
		}

		return code.str();
	}

	FalloffType mFalloffType = FalloffType::eQuadratic;
};

AQUA_END
