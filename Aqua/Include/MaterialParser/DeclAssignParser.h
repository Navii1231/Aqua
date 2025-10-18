#pragma once
#include "MaterialParserConfig.h"
#include "ExprParser.h"

AQUA_BEGIN

class DeclAssignParser
{
public:
	DeclAssignParser() = default;

	NodeAST* Parse();

	template <typename Iter>
	void SetOps(Iter begin, Iter end);

	template <typename Iter>
	void SetAssignOps(Iter begin, Iter end);

	template <typename Iter>
	void SetTokenStream(Iter begin, Iter end);

private:
	ExprParser mParser;
	ParserContext mCtx;

	size_t mStreamPlace = 0;

	std::vector<Token> mTokenStream;
	std::vector<ExpressionOperation> mOps;
	std::unordered_map<size_t, std::vector<ExpressionOperation>> mAssignOps;

	size_t mOpMaxSize = 0;

private:
	// Helper methods...

	std::vector<ExprStream> SplitTokens(); // splits the stream into tokens
	size_t MatchAssignOperator(size_t Begin); // From begin to end
	const ExpressionOperation* RetrieveOperator(const std::string& Concatenation) const;

	NodeError ConstructError(const std::string& errorInfo, int LineNo, int CharOff, ErrorType errorType);

	NodeAST* CreateNode(const std::string& name, NodeType type);

	NodeAST* ResolveLeft(const std::vector<ExprStream>& SplitedTokens);

	bool IsDecl(const ExprStream& SplitedTokens);
	std::string GetOp(const ExprStream& SplitedTokens);

	bool MatchExprPattern(const std::vector<ExprStream>& SplitedTokens);

};

template <typename Iter>
void AQUA_NAMESPACE::DeclAssignParser::SetTokenStream(Iter begin, Iter end)
{
	mStreamPlace = 0;

	mParser.SetTokenStream(begin, end);

	mTokenStream.clear();
	mTokenStream.insert(mTokenStream.begin(), begin, end);
}

template <typename Iter>
void AQUA_NAMESPACE::DeclAssignParser::SetAssignOps(Iter begin, Iter end)
{
	mAssignOps.clear();

	size_t mOpMaxSize = 0;

	for (auto curr = begin; curr != end; curr++)
	{
		size_t currSize = curr->Lexeme.size();

		mAssignOps[currSize].push_back(*curr);

		if (currSize > mOpMaxSize)
			mOpMaxSize = currSize;
	}
}

template <typename Iter>
void AQUA_NAMESPACE::DeclAssignParser::SetOps(Iter begin, Iter end)
{
	mParser.SetOps(begin, end);

	mOps.clear();
	mOps.insert(mOps.end(), begin, end);
}

AQUA_END
