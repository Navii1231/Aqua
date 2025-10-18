#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

class RGBToBWNode : public MaterialNode
{
public:
	RGBToBWNode() { mChildNodes["Color"].AddValue("Vec3", "0.0"); }

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{ return "dot(" + values[0] + ", vec3(0.2126, 0.7152, 0.0722));"; }
};

AQUA_END

