#include "Core/Aqpch.h"
#include "ScriptParser.h"

AQUA_NAMESPACE::ScriptParser::ScriptParser()
	: mExprParser()
{
	mLexer.SetWhiteSpacesAndDelimiters(" \t\r", "\n()+-*/=%|~!.");
}

void AQUA_NAMESPACE::ScriptParser::SetString(const std::string_view& code)
{
	mCode = code;
	mLexer.SetString(code);
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::Parse()
{
	if (mCode.empty())
		return nullptr;

	mShiftStack.push(CreateNode("eTranslationUnit", NodeType::eTranslationUnit));

	Advance();

	if (mCurrent.Lexeme == "function")
	{
		ParseFunction();
	}

	return nullptr;
}

void AQUA_NAMESPACE::ScriptParser::ParseFunction()
{
	/*
	* The grammar:
	* FunctionNode --> 'function' Identifier '(' FuncParsList ')' 
	* '->' Type 'begin' FuncBody 'end'
	*/

	ScopedContext ctx(this, NodeType::eFunction);

	Advance();

	Expect("function");

	auto& scope = mScopeStack.emplace_back();

	ParseIdentifier();
	Advance();

	Expect("(");
	ParseFunctionParamterList();
	Expect(")");

	Advance();

	Expect("->");
	ParseType();

	Expect("begin");
	auto cmdStmts = ParseStmtBody();

	mShiftStack.push(cmdStmts);

	Expect("end");

	ReduceFunctionNode();

	UnwindCtxErrorStack(mShiftStack.top());
}

void AQUA_NAMESPACE::ScriptParser::ParseIdentifier()
{
	/*
	* The grammar:
	* Identifier --> abcd | ABCD | 1234 | _
	*/

	ScopedContext ctx(this, NodeType::eIdentifier);

	Advance();
	NodeError identifierError;

	/**** Push the test functions here... ****/
	// TODO: Not necessary in the AST construction phase
	// TODO: Also, perhaps inefficient, later we will switch to a lambda

	std::vector<int(__cdecl*)(int)> myTests;
	myTests.push_back(isdigit);
	myTests.push_back(isalpha);
	myTests.push_back([](int c)->int { return c == '_'; });

	/******************************************/
	
	IsValidToken(mCurrent, myTests, identifierError);

	// Shift operation: Pushing the identifier node into the node stack
	std::string IdName(mCurrent.Lexeme);
	NodeAST* identifierNode = new NodeAST(IdName, NodeType::eIdentifier);
	identifierNode->ErrorInfos.emplace_back(identifierError);

	PushNode(identifierNode);

	Advance();
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseStmtBody()
{
	Advance();

	ScopedContext ctx(this, NodeType::eCmdStmts);

	NodeAST* cmdStmts = CreateNode("CmdStmts", NodeType::eCmdStmts);

	while (mCurrent.Lexeme != "end" && !mLexer.HasConsumed())
	{
		cmdStmts->Children.push_back(ParseStmt());

		// stmt end marker
		Expect("\n");
		Advance();
	}

	UnwindCtxErrorStack(cmdStmts);

	return cmdStmts;
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseStmt()
{
	/*
	* Three kind of statements are possible for now
	*	1. return statement (always begins with return and followed by an expr statement)
	*	2. while loop
	*	3. for loop
	*	4. break statement
	*	5. continue statement
	*	6. if-else statement
	*	7. Whatever remains is an expr statement
	*		-- may or may not contain an equal sign
	*/

	if(Match("return")) return  ParseReturnStatement();
	if(Match("while")) return  ParseWhileStatement();
	if(Match("for")) return  ParseForStatement();
	if(Match("break")) return ParseControlStatement(NodeType::eBreakStmt);
	if(Match("continue")) return ParseControlStatement(NodeType::eContinueStmt);
	if(Match("if")) return  ParseIfStatement();
	if(Match("else")) return  ParseElseIfOrElseStatement();

	return ParseExpr();
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseExpr()
{
	// TODO: ExprParser do not account for all booleans!

	std::vector<Token> stmtTokens;

	while (mCurrent.Lexeme != "\n")
	{
		stmtTokens.push_back(mCurrent);
		Advance();
	}

	mExprParser.SetTokenStream(stmtTokens.begin(), stmtTokens.end());

	return mExprParser.Parse();
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseReturnStatement()
{
	ScopedContext ctx(this, NodeType::eReturnStmt);

	NodeAST* returnNode = CreateNode("return", NodeType::eReturnStmt);
	returnNode->Children.push_back(ParseExpr());

	return returnNode;
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseWhileStatement()
{
	ScopedContext ctx(this, NodeType::eWhileStmt);

	NodeAST* whileNode = CreateNode("while", NodeType::eWhileStmt);
	NodeAST* boolExpr = ParseExpr();

	Advance();
	Expect("begin");

	NodeAST* whileBody = ParseStmtBody();

	Expect("end");
	Advance();

	UnwindCtxErrorStack(whileNode);

	whileNode->Children.push_back(boolExpr);
	whileNode->Children.push_back(whileBody);

	return whileNode;
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseForStatement()
{
	ScopedContext ctx(this, NodeType::eForStmt);

	NodeAST* forNode = CreateNode("for", NodeType::eForStmt);
	NodeAST* declaration = ParseExpr();

	Expect(":");
	Advance();
	NodeAST* boolExpr = ParseExpr();
	Expect(":");
	Advance();
	NodeAST* counter = ParseExpr();

	Expect("begin");

	NodeAST* forBody = ParseStmtBody();

	Expect("end");
	Advance();

	UnwindCtxErrorStack(forNode);

	forNode->Children.push_back(declaration);
	forNode->Children.push_back(boolExpr);
	forNode->Children.push_back(counter);
	forNode->Children.push_back(forBody);

	return forNode;
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseControlStatement(NodeType contStmt)
{
	if (contStmt != NodeType::eContinueStmt && contStmt != NodeType::eBreakStmt)
		return nullptr;

	ScopedContext ctx(this, contStmt);

	std::string name = contStmt == NodeType::eBreakStmt ? "break" : "continue";

	NodeAST* controlStmt = CreateNode(name, contStmt);

	Advance();
	Expect(name);

	UnwindCtxErrorStack(controlStmt);

	Advance();

	return nullptr;
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseIfStatement()
{
	return ParseIfOrElseIfStatement(NodeType::eIfStmt);
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseIfOrElseIfStatement(NodeType type)
{
	ScopedContext ctx(this, type);

	std::string name = type == NodeType::eIfStmt ? "if" : "else if";

	NodeAST* ifNode = CreateNode(name, type);
	NodeAST* condExpr = ParseExpr();

	Advance();
	Expect("begin");

	NodeAST* ifBody = ParseStmtBody();

	bool ended = Match("end");
	bool nextStmt = Match("else");

	NodeError error{};

	if (!nextStmt && !ended)
	{
		std::string lexeme(mCurrent.Lexeme);

		error = ConstructError("Unexpected token: " + lexeme, mCurrent.LineNo,
			mCurrent.CharOff, ErrorType::eSyntaxError);
	}

	Advance();

	UnwindCtxErrorStack(ifNode);

	ifNode->ErrorInfos.emplace_back(error);

	ifNode->Children.push_back(condExpr);
	ifNode->Children.push_back(ifBody);

	return ifNode;
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::ParseElseIfOrElseStatement()
{
	mLexer.SetMarker();
	auto token = mLexer.Advance();

	mLexer.RetrieveMarker();

	if (token.Lexeme == "if")
	{
		Advance();
		return ParseIfOrElseIfStatement(NodeType::eElseIfStmt);
	}

	ScopedContext ctx(this, NodeType::eElseStmt);

	NodeAST* node = CreateNode("else", NodeType::eElseStmt);

	Advance();

	NodeAST* elseNode = ParseStmtBody();

	Expect("end");
	Advance();

	UnwindCtxErrorStack(node);
	node->Children.push_back(elseNode);

	return node;
}

void AQUA_NAMESPACE::ScriptParser::ReduceFuncPar(int count)
{
	NodeAST* parCount = CreateNode("Par" + std::to_string(count), NodeType::eFuncPar);
	
	auto identifier = PopNode();
	auto type = PopNode();

	parCount->Children.push_back(type);
	parCount->Children.push_back(identifier);

	PushNode(parCount);
}

bool AQUA_NAMESPACE::ScriptParser::IsValidToken(Token& token, 
	std::vector<int(*)(int)>& myTests, NodeError& identifierError)
{
	for (char c : token.Lexeme)
	{
		bool IsValid = std::any_of(myTests.begin(), myTests.end(), [c](int(__cdecl* test)(int))->bool
		{
			return test(c) != 0;
		});

		if (!IsValid)
		{
			std::string error = "Invalid Identifier: ";
			error += token.Lexeme;

			identifierError = ConstructError(error, token.LineNo, token.CharOff, ErrorType::eSyntaxError);

			return false;
		}
	}

	return true;
}

void AQUA_NAMESPACE::ScriptParser::ParseFunctionParamterList()
{
	/*
	* The grammar:
	* FuncParsNode --> Type Identifier (',' Type Identifier)* | e
	*/

	ScopedContext ctx(this, NodeType::eFuncParList);
	int count = 0;

	Advance();

	while (mCurrent.Lexeme == "," && !mLexer.HasConsumed())
	{
		ParseType();
		ParseIdentifier();

		ReduceFuncPar(count++);

		Advance();
	}

	ReduceFuncParNodes(count);
	UnwindCtxErrorStack(mShiftStack.top());
}


void AQUA_NAMESPACE::ScriptParser::ParseType()
{
	ScopedContext ctx(this, NodeType::eType);

	Advance();
	NodeError identifierError;

	/**** Push the test functions here... ****/

	std::vector<int(__cdecl*)(int)> myTests;
	myTests.push_back(isdigit);
	myTests.push_back(isalpha);
	myTests.push_back([](int c)->int { return c == '_'; });

	/******************************************/

	IsValidToken(mCurrent, myTests, identifierError);

	// Shift operation: Pushing the identifier node into the node stack
	std::string IdName(mCurrent.Lexeme);
	NodeAST* identifierNode = CreateNode(IdName, NodeType::eType);
	identifierNode->ErrorInfos.emplace_back(identifierError);

	PushNode(identifierNode);

	Advance();
}

void AQUA_NAMESPACE::ScriptParser::ReduceFuncParNodes(int count)
{
	NodeAST* parList = CreateNode("FuncParList", NodeType::eFuncParList);

	for (int i = 0; i < count; i++)
	{
		parList->Children.push_back(PopNode());
	}

	PushNode(parList);
}

void AQUA_NAMESPACE::ScriptParser::ReduceFunctionNode()
{
	auto identifier = PopNode();
	auto parList = PopNode();
	auto returnType = PopNode();
	auto funcBody = PopNode();

	auto funcNode = CreateNode(identifier->Name, NodeType::eFunction);

	PushNode(funcNode);
}

void AQUA_NAMESPACE::ScriptParser::EnterContext(NodeType type)
{
	ParserContext ctx;
	ctx.Type = type;

	mCurrCtx.push(ctx);
}

void AQUA_NAMESPACE::ScriptParser::ExitContext()
{
	if (mCurrCtx.empty())
		return;

	auto& ctxErrors = mCurrCtx.top().Errors;

	_STL_ASSERT(ctxErrors.empty(), "ERROR: Can't remove parser context before resolving it's errors");

	mCurrCtx.pop();
}

bool AQUA_NAMESPACE::ScriptParser::Expect(const std::string& expected)
{
	std::string Concatenation;

	Concatenation += mCurrent.Lexeme;

	int LineNo = mCurrent.LineNo;
	int CharOff = mCurrent.CharOff;

	// The expected string might spread out among multiple tokens
	while (Concatenation.size() < expected.size() && !mLexer.HasConsumed())
	{
		Advance();
		Concatenation += mCurrent.Lexeme;
	}

	if (Concatenation != expected)
	{
		std::stringstream stream;
		stream << "Unexpected token at " << expected;

		PushError(stream.str(), LineNo, CharOff, ErrorType::eSyntaxError);

		return false;
	}

	return true;
}

bool AQUA_NAMESPACE::ScriptParser::Match(const std::string& rhs)
{
	std::string Concatenation;

	mLexer.SetMarker();

	Token token = mLexer.GetCurrent();
	Concatenation += token.Lexeme;

	int LineNo = token.LineNo;
	int CharOff = token.CharOff;

	// The expected string might spread out among multiple tokens
	while (Concatenation.size() < rhs.size() && !mLexer.HasConsumed())
	{
		token = mLexer.Advance();
		Concatenation += token.Lexeme;
	}

	mLexer.RetrieveMarker();

	return Concatenation == rhs;
}

void AQUA_NAMESPACE::ScriptParser::Advance()
{
	mCurrent = mLexer.Advance();
}

void AQUA_NAMESPACE::ScriptParser::PushError(const std::string& errorInfo, int LineNo,
	int CharOff, ErrorType errorType)
{
	mCurrCtx.top().Errors.push(ConstructError(errorInfo, LineNo, CharOff, errorType));
}

AQUA_NAMESPACE::NodeError AQUA_NAMESPACE::ScriptParser::ConstructError(
	const std::string& errorInfo, int LineNo, int CharOff, ErrorType errorType)
{
	NodeError error{};
	error.Message = errorInfo;
	error.LineNumber = LineNo;
	error.CharOffset = CharOff;
	error.Type = errorType;

	return error;
}

AQUA_NAMESPACE::VariableType AQUA_NAMESPACE::ScriptParser::ConvertStringToType(std::string_view String)
{
	if (String == "int") return VariableType::eInt;
	else if (String == "float") return VariableType::eFloat;
	else if (String == "vec2") return VariableType::eVec2;
	else if (String == "vec3") return VariableType::eVec3;
	else if (String == "vec4") return VariableType::eVec4;
	else if (String == "ivec2") return VariableType::eIVec2;
	else if (String == "ivec3") return VariableType::eIVec3;
	else if (String == "ivec4") return VariableType::eIVec4;
	else if (String == "imat2") return VariableType::eMat2;
	else if (String == "imat3") return VariableType::eMat3;
	else if (String == "imat4") return VariableType::eMat4;

	return VariableType::eFloat;
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::CreateNode(const std::string& name, NodeType type)
{
	return new NodeAST(name, type);
}

void AQUA_NAMESPACE::ScriptParser::PushNode(AQUA_NAMESPACE::NodeAST* node)
{
	mShiftStack.push(node);
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ScriptParser::PopNode()
{
	auto node = mShiftStack.top();
	mShiftStack.pop();

	return node;
}

void AQUA_NAMESPACE::ScriptParser::UnwindCtxErrorStack(NodeAST* node)
{
	_STL_ASSERT(!mCurrCtx.empty(), "Attempting to pop an empty ctx stack!");
	_STL_ASSERT(mCurrCtx.top().Type == node->Type, "Ctx stack type and node type didn't match!");
	
	while(!mCurrCtx.empty())
	{
		node->ErrorInfos.push_back(mCurrCtx.top().Errors.top());
		mCurrCtx.top().Errors.pop();
	}
}
