#pragma once
#include "GraphConfig.h"
#include "ParserConfig.h"
#include "../Utils/Lexer.h"

AQUA_BEGIN
EXEC_BEGIN

/*
* version string  (tick)
* resource declarations (buffers only for now)
* push constants if there are any (from dependencies, meta data 
	about parameter and translation buffer)
* user function definitions (from both dependencies and main code)
* main function and code (from op code)
*/

// this parser could be used in neural networks only...
// as for general execution graphs, we must keep the compute 
// pipeline execution as general as possible
// Look into the Scriptures/ExecutionModel.txt for more information about the parsing process
class Parser
{
public:
	Parser(std::string_view code)
		: mOpCode(code) {}

	virtual ~Parser() = default;

	// contains all extracted metadata from the code such as shared buffers, functions, global variables
	// the entry point is always a function named "Evaluate"
	virtual KernelExtractions Extract() const = 0;

protected:
	std::string_view mOpCode;
};

// a nice recursive descent parser for GLSL code
// this parser doesn't construct the compute shader but 
// only extracts the metadata needed to construct the final shader
// Extract function gives us all the needed information to construct the final GLSL code
// only works for neural networks. aqua::exec doesn't have operations in their dependencies
class GLSLParser : public Parser
{
public:
	GLSLParser(std::string_view code, uint32_t glslVersion);

	KernelExtractions Extract() const override;

private:
	std::string mGLSLVersionString;
	Lexer mLexer;

	mutable std::map<vk::DescriptorType, RscTypeHandler> mRscHandlers;

private:
	KernelExtractions ExtractMetaData(FunctionNodeType type, const std::string_view& code) const;

	FunctionNode ProcessOperationFunctionDefinition(Lexer& lexer) const;
	GraphRsc ProcessResourceDecl(Lexer& lexer) const;
	StructDefNode ProcessStructDecl(Lexer& lexer) const;

	void Expect(bool condition, const std::string& message) const;
	ConstructQualifier IdentifyConstructQualifier(Lexer& lexer) const;

	void ParseOpParameters(Lexer& lexer, FunctionNode& funcNode) const;

	void ParseFunctionBody(std::string& body, Lexer& lexer) const;
	void ParseStructBody(std::string& body, Lexer& lexer) const;
	void ParseKernelConstBody(KernelExtractions& kernel, Lexer& lexer) const;

	std::expected<TypeName, bool> GetBasicGLSLType(const Token& curr) const;
	bool IsRscDecl(std::string_view Lexeme) const;
	void ProcessCommand(KernelExtractions& kernel, Lexer& lexer) const;

private:
	// text navigation...
	void SkipToNextLine(Lexer& lexer) const;

	void SkipMultiLineComment(Lexer& lexer) const;
};

EXEC_END
AQUA_END
