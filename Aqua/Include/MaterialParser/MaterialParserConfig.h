#pragma once
#include "../Core/AqCore.h"

#include <vector>
#include <unordered_map>

#include "../Utils/Lexer.h"

AQUA_BEGIN

enum class NodeType
{
	eTranslationUnit    = 1,
	eFunction           = 2,
	eFuncPar            = 3,
	eFuncParList        = 4,
	eType               = 5,
	eIdentifier         = 6,
	eCmdStmts           = 7,
	eReturnStmt         = 8,
	eWhileStmt          = 9,
	eForStmt            = 10,
	eBreakStmt          = 11,
	eContinueStmt       = 12,
	eIfStmt             = 13,
	eElseIfStmt         = 14,
	eElseStmt           = 15,
	eFuncCall           = 16,
	eExprStmt           = 17,
	eDeclVar            = 18,
	eScopedVar          = 19,
	eMathsExpr          = 20,
	eBoolExpr           = 21,
	eIntLiteral         = 22,
	eFloatLiteral       = 23,
};

enum class SymbolCategory
{
	eFunction           = 1,
	eVariable           = 2
};

enum class VariableType
{
	eInt                = 1,
	eIVec2              = 2,
	eIVec3              = 3,
	eIVec4              = 4,
	eFloat              = 5,
	eVec2               = 6,
	eVec3               = 7,
	eVec4               = 8,
	eMat2               = 9,
	eMat3               = 10,
	eMat4               = 11,
};

enum class ErrorType
{
	eSyntaxError        = 1,
	eTypeError          = 2,
};

struct NodeError {
	std::string Message;
	int LineNumber;
	int CharOffset;
	ErrorType Type; // e.g., SyntaxError, TypeError
};

struct ParserContext
{
	NodeType Type = NodeType::eTranslationUnit;
	std::stack<NodeError> Errors;
};

struct NodeAST
{
	std::string Name = "TranslationUnit";
	NodeType Type = NodeType::eTranslationUnit;
	std::vector<NodeError> ErrorInfos;

	std::vector<NodeAST*> Children;

	NodeAST() = default;

	NodeAST(const std::string& name, NodeType type)
		: Name(name), Type(type) {}
};

struct ScriptSymbol
{
	std::string Name;
	std::string Scope;

	VariableType VarType = VariableType::eFloat; // Works as return type for a function
	SymbolCategory Category = SymbolCategory::eVariable;

	std::vector<VariableType> FunctionPars;
};

struct ExpressionOperation
{
	std::string Lexeme;
	int32_t Precedence = 1;
	int32_t OperandCount = 1;
	NodeType OpType = NodeType::eMathsExpr;

	ExpressionOperation() = default;
	ExpressionOperation(const std::string& lexeme, int32_t precedence,
		int32_t operandCount, NodeType opType)
		: Lexeme(lexeme), Precedence(precedence),
		OperandCount(operandCount), OpType(opType) {}
};

enum Precedence : int32_t
{
	eFuncCallPrec  = -1,
	eBracketPrec  = -1,
	eCommaPrec  = 0,
};

inline ExpressionOperation CreateBracketOp() { return { "(", eBracketPrec, INT32_MAX, NodeType::eMathsExpr }; }
inline ExpressionOperation CreateCommaOp() { return { "(", eCommaPrec, INT32_MAX, NodeType::eMathsExpr }; }

inline ExpressionOperation CreateFunctionOp(const std::string_view& name)
{ return { std::string(name), eFuncCallPrec, INT32_MAX, NodeType::eFuncCall}; }


using ExprOperList = std::vector<ExpressionOperation>;
// Variable/Function Lexeme  -->  Symbol
using SymbolTable = std::unordered_map<std::string, ScriptSymbol>;

AQUA_END
