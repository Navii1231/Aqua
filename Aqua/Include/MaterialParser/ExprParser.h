#pragma once
#include "MaterialParserConfig.h"

#include <stack>
#include <vector>

AQUA_BEGIN

struct ExprStream
{
	size_t Begin = 0;
	size_t End = 0;

	bool IsAssignment = false;
};

// Parses the expr stmts...
// TODO: Function calls have to be handled separately
// We need some major changes in the structure to implement that
class ExprParser
{
public:
	ExprParser() = default;

	ExprParser(const ExprParser&) = delete;
	ExprParser operator=(const ExprParser&) = delete;

	NodeAST* ParseExpr();

	ParserContext GetParserContext() const { return mCtx; }

	template <typename Iter>
	void SetOps(Iter begin, Iter end);

	template <typename Iter>
	void SetTokenStream(Iter begin, Iter end);

	void SetExprStream(const ExprStream& stream) { mStreamWindow = stream; }

	void Reset();

	static constexpr int sMaxOperators = 3;
	
private:
	ExprStream mStreamWindow;
	// TODO: Use iterators instead of the vector
	// Keep Begin and End iterators of the tokens
	std::vector<Token> mTokens;

	// State
	Token mCurrent;
	size_t mPlace = 0;

	ParserContext mCtx;

	std::stack<NodeAST*> mOperandStack;
	std::stack<ExpressionOperation> mOperatorStack;

	// Length of the operator lexeme --> ExpressionOperation
	std::unordered_map<size_t, std::vector<ExpressionOperation>> mOpsTable;
	// Maximum length stored operator lexemes
	size_t mMaxLength = 0;

private:
	// Helper functions...
	bool Expect(const std::string& expected);
	bool Match(const std::string& rhs);
	void Advance();

	bool Reduce();
	void ReduceUntilLowerPrecedenceHits(int32_t level);
	void ReduceEntireOperatorStack();

	// TODO: Implement a function here which handles a function call
	// It has to be recursive as the argument themselves 
	// nothing but expressions...
	void ParseFunction();

	void ConstructError(const std::string& errorInfo, int LineNo, int CharOff, ErrorType errorType);
	void ConstructErrorAndRetrieveLexer(const std::string& errorInfo,
		int LineNo, int CharOff, ErrorType errorType, size_t marker);

	bool ParseNumericLiteral();
	bool ContainFloatNumericLiterals(const std::string_view& lexeme);
	bool ContainIntNumericLiterals(const std::string_view& lexeme);

	const ExpressionOperation* SeekOperator();
	const ExpressionOperation* RetrieveOperator(const std::string& Concatenation) const;

	bool IsValidToken(const Token& token, const std::vector<int(__cdecl*)(int)>& myTests);
	bool IsValidIdentifier(const Token& token);

	void ParseNonOperator();

	static NodeAST* CreateNode(const std::string& name, NodeType type);
};

template <typename Iter>
void AQUA_NAMESPACE::ExprParser::SetTokenStream(Iter begin, Iter end)
{
	_STL_ASSERT(begin != end, "Expr stream can't be empty!");

	mTokens.clear();
	mTokens.insert(mTokens.begin(), begin, end);

	Reset();
}

template <typename Iter>
void AQUA_NAMESPACE::ExprParser::SetOps(Iter begin, Iter end)
{
	mOpsTable.clear();

	mMaxLength = 0;

	for (; begin != end; begin++)
	{
		size_t length = begin->Lexeme.size();
		mOpsTable[length].push_back(*begin);

		if (mMaxLength < length)
			mMaxLength = length;
	}
}

AQUA_END
