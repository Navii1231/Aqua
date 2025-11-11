#include "Core/Aqpch.h"
#include "Execution/Parser.h"

AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::GLSLParser(std::string_view code, uint32_t glslVersion) : Parser(code)
{
	mGLSLVersionString = "#version " + std::to_string(glslVersion) + "\n";

	mRscHandlers[vk::DescriptorType::eStorageBuffer].Extract = [this](Lexer& lexer, GraphRsc& rscNode)
		{
			lexer++;
			Expect(lexer++ == "<", "expecting <");

			rscNode.Typename = lexer++;
			Expect(lexer++ == ">", "expecting >");

			// name of the buffer
			rscNode.Name = lexer++;

			// no, there's a constructor taking set/binding
			Expect(lexer++ == "(", "Expected '(' after shared_buffer name");

			// set
			Expect(GetTypename(*lexer) != TypeName::eInvalid, "Expected set number");
			rscNode.Location.SetIndex = std::stoi(lexer++);

			Expect(lexer++ == ",", "Expected ',' after set number");

			// binding
			Expect(GetTypename(*lexer) != TypeName::eInvalid, "Expected binding number");
			rscNode.Location.Binding = std::stoi(lexer++);

			Expect(lexer++ == ")", "Expected ')' after binding number");
			// we have the buffer declaration -- store it
			// expecting a semicolon

			Expect(*lexer == ";", "Expected ';' after shared_buffer declaration");
		};

	mRscHandlers[vk::DescriptorType::eUniformBuffer] = mRscHandlers[vk::DescriptorType::eStorageBuffer];
	mRscHandlers[vk::DescriptorType::eStorageImage] = mRscHandlers[vk::DescriptorType::eStorageBuffer];
	mRscHandlers[vk::DescriptorType::eSampledImage] = mRscHandlers[vk::DescriptorType::eStorageBuffer];

	mRscHandlers[vk::DescriptorType::eStorageBuffer].Typename = "shared_buffer";
	mRscHandlers[vk::DescriptorType::eUniformBuffer].Typename = "uniform_buffer";
	mRscHandlers[vk::DescriptorType::eSampledImage].Typename = "sampled_image";
	mRscHandlers[vk::DescriptorType::eStorageImage].Typename = "image";
}

AQUA_NAMESPACE::EXEC_NAMESPACE::KernelExtractions AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::Extract() const
{
	return ExtractMetaData(FunctionNodeType::eOpTransform, mOpCode);
}

AQUA_NAMESPACE::EXEC_NAMESPACE::KernelExtractions AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ExtractMetaData(FunctionNodeType type, const std::string_view& code) const
{
	std::string whiteSpaces = " \r\t\n", coarseWhiteSpaces = " \r\t";
	std::string delimiters = ";,.{}[]()=+-*/&^%~!<>?:", coarseDelimiters = ";(){}[]<>/*\n";

	KernelExtractions exts{};
	exts.NodeType = type;

	// huh, need a error propagation system here
	Lexer lexer;
	lexer.SetString(code);
	lexer.SetWhiteSpacesAndDelimiters(coarseWhiteSpaces, coarseDelimiters);
	lexer++; // at the first token of the string...

	while (!lexer.HasConsumed())
	{
		// looking for resource declarations, or function definitions
		lexer.SetWhiteSpacesAndDelimiters(coarseWhiteSpaces, coarseDelimiters);

		if (*lexer == "/")
		{
			if (lexer[lexer.GetPosition() + 1] == '/')
			{
				SkipToNextLine(lexer);
				lexer++;
				continue;
			}
			else if (lexer[lexer.GetPosition() + 1] == '*')
			{
				SkipMultiLineComment(lexer);
				lexer++;
				continue;
			}
		}

		// here we use a small lookahead to identify the construct type
		// could be done with shift-reduce parsing, matching the patterns
		ConstructQualifier qualifier = IdentifyConstructQualifier(lexer);

		FunctionNode funcNode{};
		StructDefNode structNode{};
		GraphRsc rsc{};

		// once the qualifier is identified, we can process the construct accordingly
		switch (qualifier)
		{
			case ConstructQualifier::eNone:
				// eh, nothing to do here, empty node
				lexer++;
				break;
			case ConstructQualifier::eGlobalVar:
				// process global variable declaration node
				lexer++;
				break;
			case ConstructQualifier::eFunctionDefinition:
				// function definition node
				lexer.SetWhiteSpacesAndDelimiters(whiteSpaces, delimiters);
				funcNode = ProcessOperationFunctionDefinition(lexer);

				if (funcNode.Name == "Evaluate")
					exts.EvaluateFunction = funcNode;
				else
					exts.Functions.push_back(funcNode);

				lexer++;
				break;
			case ConstructQualifier::eStructDefinition:
				// struct definition
				lexer.SetWhiteSpacesAndDelimiters(whiteSpaces, delimiters);
				structNode = ProcessStructDecl(lexer);
				exts.StructDefs.push_back(structNode);

				lexer++;
				break;
			case ConstructQualifier::eRscDecl:
				// shared buffer declaration node
				lexer.SetWhiteSpacesAndDelimiters(whiteSpaces, delimiters);
				rsc = ProcessResourceDecl(lexer);
				exts.Rscs[rsc.Location] = rsc;

				lexer++;
				break;
			case ConstructQualifier::eComputeParDecl:
				// declaring how compute parameters like work group size and invocation count
				lexer.SetWhiteSpacesAndDelimiters(whiteSpaces, delimiters);
				ProcessCommand(exts, lexer);

				lexer++;
				break;
			case ConstructQualifier::eInvalid:
				// skipping to the next line
				SkipToNextLine(lexer);
				lexer++;

				break;
			default:
				break;
		};
	}

	return exts;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::FunctionNode AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ProcessOperationFunctionDefinition(Lexer& lexer) const
{
	FunctionNode funcNode;
	funcNode.Type = FunctionNodeType::eOpTransform;

	// return type
	lexer++;

	// function name
	funcNode.Name = std::string(lexer++);

	// opening parameters
	Expect(lexer++ == "(", "Expected '(' after function name");

	// parse parameters
	// a normal parameter is defined as: type name
	// the special parameter is defined as: MACRO type name
	// MACRO -> text | text ( identifiers ) | ε
	// identifiers -> identifier, identifiers | identifier | ε
	ParseOpParameters(lexer, funcNode);
	lexer++;

	// expecting ->return_type
	auto arrowLine = lexer++;
	auto arrowHead = lexer++;

	Expect(arrowLine == "-" && arrowHead == ">", "expecting '->'");

	funcNode.ReturnType = *GetTypename(std::string(lexer++));

	Expect(lexer++ == "{", "Expected '{' to start function body");

	// parse function body
	// keeping track of the braces to find the end of the function body
	ParseFunctionBody(funcNode.Body, lexer);

	// ooh la la, we have the function definition

	return funcNode;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::GraphRsc AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ProcessResourceDecl(Lexer& lexer) const
{
	GraphRsc rscNode;

	// found shared buffer -- process it

	if (*lexer == "shared_buffer") // storage buffer type
		rscNode.Type = vk::DescriptorType::eStorageBuffer;
	else if (*lexer == "uniform_buffer")
		rscNode.Type = vk::DescriptorType::eUniformBuffer;
	else if (*lexer == "sampled_image")
		rscNode.Type = vk::DescriptorType::eSampledImage;
	else if (*lexer == "image") // storage image
		rscNode.Type = vk::DescriptorType::eStorageImage;

	else
		Expect(false, "invalid resource declaration");

	mRscHandlers[rscNode.Type].Extract(lexer, rscNode);

	return rscNode;
}

AQUA_NAMESPACE::EXEC_NAMESPACE::StructDefNode AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ProcessStructDecl(Lexer& lexer) const
{
	StructDefNode node{};
	lexer++;
	node.Typename = lexer++;

	Expect(lexer++ == "{", "expecting '{' after struct declaration");

	ParseStructBody(node.Body, lexer);

	Expect(*(++lexer) == ";", "missing semicolon after struct declaration");

	return node;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::Expect(bool condition, const std::string& message) const
{
	// halting the program right away for now...
	_STL_ASSERT(condition, message.c_str());
}

AQUA_NAMESPACE::EXEC_NAMESPACE::ConstructQualifier AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::IdentifyConstructQualifier(Lexer& lexer) const
{
	if (lexer.HasConsumed())
		return ConstructQualifier::eInvalid;

	// look ahead a few tokens to identify the construct
	Token first = *lexer;

	if (lexer.HasConsumed())
		return ConstructQualifier::eInvalid;

	Token second = lexer + 1;
	// simple heuristics to identify the construct
	if (IsRscDecl(first))
	{
		return ConstructQualifier::eRscDecl;
	}
	if (first == "function")
	{
		return ConstructQualifier::eFunctionDefinition;
	}
	if (first == "struct")
	{
		return ConstructQualifier::eStructDefinition;
	}
	if (second == ";")
	{
		return ConstructQualifier::eGlobalVar;
	}
	if (first.size() == 3)
	{
		std::string command = std::string(first);

		std::for_each(command.begin(), command.end(), [](char& c)
			{
				c = std::tolower(c);
			});

		if (command == "set")
			return ConstructQualifier::eComputeParDecl;
	}

	return ConstructQualifier::eInvalid;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ParseOpParameters(Lexer& lexer, FunctionNode& funcNode) const
{
	while (*lexer != ")" && !lexer.HasConsumed())
	{
		// check for special parameter macros
		if (*lexer == "AQUA_DEPENDENCIES")
		{
			// aqua parameters
			lexer++;
			TypeName paramType = *GetTypename(std::string(lexer++));
			funcNode.AquaDependencyName = std::string(*lexer);
		}
		else
		{
			// normal parameter
			TypeName paramType = *GetTypename(std::string(lexer++));
			std::string paramName = std::string(lexer++);

			Expect(funcNode.Parameters.find(paramName) == funcNode.Parameters.end(), "duplicate function parameter");

			funcNode.Parameters[paramName] = paramType;
		}
		// check for comma or ending parameters
		if (*lexer == ",")
		{
			// continue to next parameter
			lexer++;
			continue;
		}
		if (*lexer == ")")
		{
			// end of parameters
			break;
		}
		else
		{
			Expect(false, "Expected ',' or ')' after function parameter");
		}
	}
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ParseFunctionBody(std::string& body, Lexer& lexer) const
{
	auto mainString = lexer.GetString();

	int braceCount = 1;
	body.clear();

	while (braceCount > 0 && !lexer.HasConsumed())
	{
		size_t begin = lexer.GetPosition();

		if (*lexer == "{")
		{
			braceCount++;
		}
		else if (*lexer == "}")
		{
			braceCount--;
			if (braceCount == 0)
			{
				// reached the end of the function body
				break;
			}
		}

		lexer++;

		size_t end = lexer.GetPosition();

#if 0 // no replacement of basic types, instead we'll be using macros
		std::expected<TypeName, bool> basicType = GetBasicGLSLType(curr);

		if (basicType)
		{
			end -= curr.size();
			body += mainString.substr(begin, end - begin);
			body += *ConvertToGLSLString(*basicType);
			continue; // looping back
		}
#endif

		// append the current lexeme to the function body
		body += mainString.substr(begin, end - begin);
	}

	// thank you GLSL for always having balanced braces
	Expect(braceCount == 0, "Unbalanced braces in function body");

	// ending function body
	Expect(*lexer == "}", "Expected '}' to end function body");
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ParseStructBody(std::string& body, Lexer& lexer) const
{
	ParseFunctionBody(body, lexer);
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ParseKernelConstBody(KernelExtractions& kernel, Lexer& lexer) const
{
	auto& pushConsts = kernel.KernelConsts;
	uint32_t idx = 0;

	Expect(pushConsts.empty(), "you can't define the kernel constants more than once");

	while (*lexer != "}" && !lexer.HasConsumed())
	{
		TypeName type = *GetTypename(lexer++);
		std::string name = lexer++;

		auto& kernelConst = pushConsts.emplace_back();

		kernelConst.Type = type;
		kernelConst.FieldName = name;
		kernelConst.Idx = idx++;

		Expect(lexer++ == ";", "missing semicolon");
	}

	Expect(*(++lexer) == ";", "missing semicolon");
}

std::expected<AQUA_NAMESPACE::EXEC_NAMESPACE::TypeName, bool> AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::GetBasicGLSLType(const Token& curr) const
{
	return GetTypename(std::string(curr));
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::IsRscDecl(std::string_view Lexeme) const
{
	for (const auto& [descType, handler] : mRscHandlers)
	{
		if (handler.Typename == Lexeme)
			return true;
	}

	return false;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::ProcessCommand(KernelExtractions& kernel, Lexer& lexer) const
{
	lexer++;

	if (*lexer == "WorkGroupSize")
	{
		lexer++;
		Expect(lexer++ == "=", "expecting and equal sign");
		Expect(lexer++ == "{", "expecting and equal sign");

		kernel.WorkGroupSize.x = std::stoi(lexer++);
		Expect(lexer++ == ",", "',' expected after the first argument");
		kernel.WorkGroupSize.y = std::stoi(lexer++);
		Expect(lexer++ == ",", "',' expected after the second argument");
		kernel.WorkGroupSize.z = std::stoi(lexer++);

		Expect(*lexer == "}", "missing closing brace");
	}
	else if (*lexer == "InvocationCount")
	{
		lexer++;
		Expect(lexer++ == "=", "expecting and equal sign");
		Expect(lexer++ == "{", "expecting and equal sign");

		kernel.InvocationCount.x = std::stoi(lexer++);
		Expect(lexer++ == ",", "',' expected after the first argument");
		kernel.InvocationCount.y = std::stoi(lexer++);
		Expect(lexer++ == ",", "',' expected after the second argument");
		kernel.InvocationCount.z = std::stoi(lexer++);

		Expect(*lexer == "}", "missing closing brace");
	}
	else if (*lexer == "KernelConsts")
	{
		lexer++;
		Expect(lexer++ == "{", "expected '{'");
		ParseKernelConstBody(kernel, lexer);
	}
	else
	{
		Expect(false, "invalid compute command");
	}
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::SkipToNextLine(Lexer& lexer) const
{
	while (*lexer != "\n" && !lexer.HasConsumed())
		lexer++;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLParser::SkipMultiLineComment(Lexer& lexer) const
{
	while (true)
	{
		while (*lexer != "*" && !lexer.HasConsumed())
			lexer++;

		if (lexer.HasConsumed())
			break;

		if (lexer[lexer.GetPosition() + 1] == '/')
			break;
	}
}
