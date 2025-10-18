#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

enum class VectorOp 
{
	eAdd,
	eSubtract,
	eCross,
	eDot,
	eNormalize
};

class VectorMathNode : public MaterialNode 
{
public:
	VectorMathNode() {
		mChildNodes["Vector1"].AddValue("Vec3", "0.0");
		mChildNodes["Vector2"].AddValue("Vec3", "0.0");
	}

	std::string EvaluateVector(const std::string& a, const std::string& b) const 
	{
		switch (mOperation) {
			case VectorOp::eAdd: return a + " + " + b;
			case VectorOp::eSubtract: return a + " - " + b;
			case VectorOp::eCross: return "cross(" + a + ", " + b + ")";
			case VectorOp::eDot: return "vec3(dot(" + a + ", " + b + "))";
			case VectorOp::eNormalize: return "normalize(" + a + ")";
			default: return "vec3(0.0)";
		}
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		std::stringstream code;
		code << EvaluateVector(values[0], values[1]) << ";";
		return code.str();
	}

	VectorOp mOperation = VectorOp::eAdd;
};

AQUA_END
