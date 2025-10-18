#include "Core/aqpch.h"
#include "DeclAssignParser.h"

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::DeclAssignParser::Parse()
{
	// Find the assignment operators
	auto SplitedTokens = SplitTokens();

	if (SplitedTokens.size() == 1)
	{
		// Only maths expr and no assignment
		mParser.SetExprStream(SplitedTokens[0]);

		return mParser.ParseExpr();
	}

	if (SplitedTokens.size() == 3)
	{
		// There is an assignment operator, it must be in the middle

		if (!MatchExprPattern(SplitedTokens))
		{
			NodeError error = ConstructError("invalid expr", mTokenStream[0].LineNo,
				mTokenStream[0].CharOff, ErrorType::eSyntaxError);

			NodeAST* node = CreateNode("expr", NodeType::eExprStmt);
			node->ErrorInfos.push_back(error);

			return node;
		}

		NodeAST* node = ResolveLeft(SplitedTokens);

		mParser.SetExprStream(SplitedTokens[3]);
		NodeAST* rhs = mParser.ParseExpr();

		node->Children.push_back(rhs);

		return node;
	}

	NodeError error = ConstructError("invalid expr", mTokenStream[0].LineNo,
		mTokenStream[0].CharOff, ErrorType::eSyntaxError);

	NodeAST* node = CreateNode("expr", NodeType::eExprStmt);
	node->ErrorInfos.push_back(error);

	return node;
}

std::vector<AQUA_NAMESPACE::ExprStream> AQUA_NAMESPACE::DeclAssignParser::SplitTokens()
{
	std::vector<ExprStream> stream;

	size_t Begin = 0;

	for (size_t i = 0; i < mTokenStream.size(); i++)
	{
		size_t End = MatchAssignOperator(i);

		if (Begin != End)
		{
			stream.push_back({ Begin, i, false });
			stream.push_back({ i, End, true});

			i = End;
		}
	}

	ExprStream str{ 0, 0, false };
	str.Begin = Begin;
	str.End = mTokenStream.size();

	stream.push_back(str);

	return stream;
}

size_t AQUA_NAMESPACE::DeclAssignParser::MatchAssignOperator(size_t Begin)
{
	size_t temp = Begin;
	Token token = mTokenStream[Begin++];

	if (token.Lexeme.size() > mOpMaxSize)
	{
		return temp;
	}

	std::string Concatenation;
	const ExpressionOperation* op = nullptr;

	while (true)
	{
		Concatenation += token.Lexeme;

		if (Concatenation.size() > mOpMaxSize)
			break;

		const ExpressionOperation* newOp = RetrieveOperator(Concatenation);

		if (!newOp)
		{
			op = newOp;
			temp = Begin;
		}

		token = mTokenStream[++Begin];
	}

	return temp;
}

const AQUA_NAMESPACE::ExpressionOperation* AQUA_NAMESPACE::DeclAssignParser::
	RetrieveOperator(const std::string& Concatenation) const
{
	auto& operators = mAssignOps.at(Concatenation.size());

	const ExpressionOperation* opFound = nullptr;

	bool found = std::any_of(operators.begin(), operators.end(),
		[&Concatenation, &opFound](const ExpressionOperation& op)
	{
		if (op.Lexeme == Concatenation)
		{
			opFound = &op;
			return true;
		}

		return false;
	});

	return opFound;
}

AQUA_NAMESPACE::NodeError AQUA_NAMESPACE::DeclAssignParser::ConstructError(
	const std::string& errorInfo, int LineNo, int CharOff, ErrorType errorType)
{
	NodeError error{};
	error.Message = errorInfo;
	error.LineNumber = LineNo;
	error.CharOffset = CharOff;
	error.Type = errorType;

	return error;
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::DeclAssignParser::CreateNode(const std::string& name, NodeType type)
{
	return new NodeAST(name, type);
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::DeclAssignParser::ResolveLeft(const std::vector<ExprStream>& SplitedTokens)
{
	bool isDecl = IsDecl(SplitedTokens[0]);
	std::string op = GetOp(SplitedTokens[1]);

	NodeAST* opStmt = CreateNode(op, NodeType::eExprStmt);

	if (isDecl)
	{
		std::string typeLexeme(mTokenStream[0].Lexeme);
		std::string varLexeme(mTokenStream[1].Lexeme);

		NodeAST* type = CreateNode(typeLexeme, NodeType::eType);
		NodeAST* var = CreateNode(varLexeme, NodeType::eDeclVar);

		opStmt->Children.push_back(type);
		opStmt->Children.push_back(var);
	}
	else
	{
		std::string varLexeme(mTokenStream[0].Lexeme);

		NodeAST* scopedVar = CreateNode(varLexeme, NodeType::eScopedVar);
		opStmt->Children.push_back(scopedVar);
	}

	return opStmt;
}

bool AQUA_NAMESPACE::DeclAssignParser::IsDecl(const ExprStream& SplitedTokens)
{
	size_t diff = SplitedTokens.End - SplitedTokens.Begin;

	if (diff == 2 || diff == 1)
	{
		return diff == 2;
	}

	mCtx.Errors.push(ConstructError("Invalid statement!", mTokenStream[0].LineNo,
		mTokenStream[0].CharOff, ErrorType::eSyntaxError));

	return false;
}

std::string AQUA_NAMESPACE::DeclAssignParser::GetOp(const ExprStream& SplitedTokens)
{
	std::string concatenate{};

	for (size_t i = SplitedTokens.Begin; i < SplitedTokens.End; i++)
	{
		concatenate += mTokenStream[i].Lexeme;
	}

	return concatenate;
}

bool AQUA_NAMESPACE::DeclAssignParser::MatchExprPattern(const std::vector<ExprStream>& SplitedTokens)
{
	if (!(!SplitedTokens[0].IsAssignment &&
		SplitedTokens[1].IsAssignment &&
		!SplitedTokens[2].IsAssignment))
		return false;

	size_t diff = SplitedTokens[0].End - SplitedTokens[0].Begin;

	if (diff != 1 || diff != 2)
		return false;

	return true;
}
