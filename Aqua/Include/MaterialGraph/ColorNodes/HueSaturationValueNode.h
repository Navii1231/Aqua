#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

// Will be generated a function out of it
class HSVNode : public MaterialNode {
public:
	HSVNode() 
	{
		mChildNodes["Color"].AddValue("Vec3", "0.0");
		mChildNodes["Hue"].AddValue("Float", "0.0");
		mChildNodes["Saturation"].AddValue("Float", "1.0");
		mChildNodes["Value"].AddValue("Float", "1.0");
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		std::stringstream code;
		code << "vec3 col = " << values[0] << ";";
		code << "float h = " << values[1] << ";";
		code << "float s = " << values[2] << ";";
		code << "float v = " << values[3] << ";";
		code << "vec3 hsv = rgb2hsv(col);";
		code << "hsv.x += h; hsv.y *= s; hsv.z *= v;";
		code << "vec3 result = hsv2rgb(hsv);";
		return code.str();
	}
};

AQUA_END
