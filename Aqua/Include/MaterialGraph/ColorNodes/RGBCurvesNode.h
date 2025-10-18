#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

// Will be generated a function out of it
class RGBCurvesNode : public MaterialNode
{
public:
	RGBCurvesNode() 
	{
		mChildNodes["Color"].AddValue("Vec3", "0.0");
		mChildNodes["RedCurve"].AddValue("Sampler1D", "0.0");
		mChildNodes["GreenCurve"].AddValue("Sampler1D", "0.0");
		mChildNodes["BlueCurve"].AddValue("Sampler1D", "0.0");
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		std::stringstream code;
		code << "vec3 col = " << values[0] << ";";
		code << "float r = texture(" << values[1] << ", col.r).r;";
		code << "float g = texture(" << values[2] << ", col.g).r;";
		code << "float b = texture(" << values[3] << ", col.b).r;";
		code << "vec3 result = vec3(r, g, b);";
		return code.str();
	}
};


AQUA_END
