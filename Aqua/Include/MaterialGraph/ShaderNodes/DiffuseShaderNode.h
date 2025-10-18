#pragma once
#include "../MaterialNode.h"

AQUA_BEGIN

class DiffuseShaderNode : public MaterialNode
{
public:
	DiffuseShaderNode()
	{
		mChildNodes["BaseColor"].AddValue("Float", "0.4");
		mChildNodes["Roughness"].AddValue("Float", "0.4");
	}

	virtual std::string GenerateCode(const SocketValues& input) const override
	{
		std::string sampleInfo = input[0];
		std::string Direction = input[1];
		std::string BaseColor = input[2];
		std::string Roughness = input[3];

		std::stringstream code;

		code << 

		code << "LambertianBRDF(" << sampleInfo << ".iNormal, " << Direction 
			<< ", " << BaseColor << ")";

		return code.str();
	}

private:

};

AQUA_END
