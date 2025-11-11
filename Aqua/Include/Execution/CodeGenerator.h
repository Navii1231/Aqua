#pragma once
#include "ParserConfig.h"

AQUA_BEGIN
EXEC_BEGIN

class CodeGenerator
{
public:
	CodeGenerator(const KernelExtractions& exts)
		: mExts(exts) {}

	virtual std::string Generate() const = 0;

protected:
	KernelExtractions mExts;
};

class GLSLCodeGenerator : public CodeGenerator
{
public:
	GLSLCodeGenerator(const KernelExtractions& exts)
		: CodeGenerator(exts) {}

	virtual std::string Generate() const override;

private:
	void InsertRscDecl(std::stringstream& stream, const GraphRsc& rsc) const;
};

EXEC_END
AQUA_END
