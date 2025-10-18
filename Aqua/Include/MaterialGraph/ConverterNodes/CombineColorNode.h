#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

class CombineColorNode : public MaterialNode 
{
public:
	CombineColorNode() {
		mChildNodes["R"].AddValue("Float", "0.0");
		mChildNodes["G"].AddValue("Float", "0.0");
		mChildNodes["B"].AddValue("Float", "0.0");
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		return "vec3(" + values[0] + ", " + values[1] + ", " + values[2] + ");";
	}
};

AQUA_END

