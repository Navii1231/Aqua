#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

class SeparateColorNode : public MaterialNode
{
public:
	SeparateColorNode() { mChildNodes["Color"].AddValue("Vec3", "0.0"); }

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		// Doesn't need to generate a code
		std::stringstream code;
		code << "vec3 Color = " << values[0] << ";";
		code << "float R = Color.r;";
		code << "float G = Color.g;";
		code << "float B = Color.b;";
		return code.str();
	}
};

AQUA_END
