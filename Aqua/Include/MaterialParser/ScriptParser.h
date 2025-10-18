#pragma once
#include "MaterialParserConfig.h"
#include "ExprParser.h"
#include "DeclAssignParser.h"

AQUA_BEGIN

class ScriptParser
{
public:
	ScriptParser();

	void SetString(const std::string_view& code);

	NodeAST* Parse();

	ParserContext GetParserContext() const { mCurrCtx.empty() ? ParserContext() : mCurrCtx.top(); }
private:
	std::string_view mCode;

	Lexer mLexer;
	DeclAssignParser mExprParser;

	std::stack<NodeAST*> mShiftStack;
	std::stack<ParserContext> mCurrCtx;

	std::vector<SymbolTable> mScopeStack;

	Token mCurrent;

private:
	// Control flow parsing
	void ParseFunction();
	void ParseFunctionParamterList();
	void ParseType();
	void ParseIdentifier();

	// Stmt level parsing...
	NodeAST* ParseStmtBody();
	NodeAST* ParseStmt();
	NodeAST* ParseExpr();

	NodeAST* ParseReturnStatement();
	NodeAST* ParseWhileStatement();
	NodeAST* ParseForStatement();
	NodeAST* ParseControlStatement(NodeType contStmt);
	NodeAST* ParseIfStatement();
	NodeAST* ParseIfOrElseIfStatement(NodeType type);
	NodeAST* ParseElseIfOrElseStatement();

	// Dispatching
	template <typename Fn>
	NodeAST* DispatchStmt(const std::string& marker, Fn fn);

	// Reduction steps...
	void ReduceFuncPar(int count);
	void ReduceFuncParNodes(int count);
	void ReduceFunctionNode();

	bool IsValidToken(Token& token, std::vector<int(*)(int)>& myTests, NodeError& identifierError);

	// Context and state management
	void EnterContext(NodeType ctx);
	void ExitContext();

	// Token checking and advance logic...
	// The lexer can advance while expecting, 
	// however it is guaranteed to retain it's state
	// while matching a token...
	bool Expect(const std::string& expected);
	bool Match(const std::string& rhs);
	void Advance();

	void PushError(const std::string& errorInfo, int LineNo, int CharOff, ErrorType errorType);
	NodeError ConstructError(const std::string& errorInfo, int LineNo, int CharOff, ErrorType errorType);

	class ScopedContext
	{
	public:
		ScopedContext(ScriptParser* parser, NodeType ctx) : mMyself(parser) { mMyself->EnterContext(ctx); }
		~ScopedContext() { mMyself->ExitContext(); }

	private:
		ScriptParser* mMyself = nullptr;
	};

	// Helper functions...
	VariableType ConvertStringToType(std::string_view String);

	static NodeAST* CreateNode(const std::string& name, NodeType type);

	void PushNode(NodeAST* node);
	NodeAST* PopNode();

	void UnwindCtxErrorStack(NodeAST* node);
};

template <typename Fn>
NodeAST* AQUA_NAMESPACE::ScriptParser::DispatchStmt(const std::string& marker, Fn fn)
{
	if (mCurrent.Lexeme == marker)
		return fn();

	return nullptr;
}

AQUA_END
