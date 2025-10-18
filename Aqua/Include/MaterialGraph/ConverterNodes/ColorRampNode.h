#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

// Will be generated a function out of it
class ColorRampNode : public MaterialNode
{
public:
	ColorRampNode()
	{
		mChildNodes["HighVal"].AddValue("Float", "1.0");
		mChildNodes["LowVal"].AddValue("Float", "0.0");
		mChildNodes["HighColor"].AddValue("Vec3", "0.0");
		mChildNodes["LowColor"].AddValue("Vec3", "0.0");
		mChildNodes["Fac"].AddValue("Float", "1.0");
	}

	virtual std::string GenerateCode(const SocketValues & values) const
	{
		std::stringstream code;
		code << "if(x < a) return ac";
		code << "if(x > b) return bc";
		code << "float a = " << values[0] << ";";
		code << "float b = " << values[1] << ";";
		code << "vec3 ac = " << values[2] << ";";
		code << "vec3 bc = " << values[3] << ";";

		code << "x = " << values[4] << ";";

		code << "vec3 result = ((bc - ac) * x + b * ac - a * bc) / (b - a);";

		// What are we gonna do with the result value?
		return code.str();
	}
};

AQUA_END
