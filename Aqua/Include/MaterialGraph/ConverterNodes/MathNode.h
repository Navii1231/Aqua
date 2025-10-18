#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

enum class MathOperation {
	eAdd, eSubtract, eMultiply, eDivide,
	ePower, eModulo, eMin, eMax
};

class MathNode : public MaterialNode {
public:
	MathNode() 
	{
		mChildNodes["Value1"].AddValue("Float", "0.0");
		mChildNodes["Value2"].AddValue("Float", "0.0");
	}

	virtual std::string GenerateCode(const SocketValues& values) const override 
	{
		std::stringstream code;

		const std::string& a = values[0];
		const std::string& b = values[1];

		switch (mMathOp) {
			case MathOperation::eAdd: code << a << " + " << b; break;
			case MathOperation::eSubtract: code << a << " - " << b; break;
			case MathOperation::eMultiply: code << a << " * " << b; break;
			case MathOperation::eDivide: code << a << " != 0.0f ? " <<
				a << " / " << b << " : 0.0f"; break;
			case MathOperation::ePower: code << "pow(" << a << ", " << b << ")"; break;
			case MathOperation::eModulo: code << "mod(" << a << ", " << b << ")"; break;
			case MathOperation::eMin: code << "min(" << a << ", " << b << ")"; break;
			case MathOperation::eMax: code << "max(" << a << ", " << b << ")"; break;
			default: code <<  "0.0";
		}

		return code.str();
	}

	MathOperation mMathOp = MathOperation::eAdd;
};

AQUA_END
