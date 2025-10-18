#include "Core/Aqpch.h"
#include "Core/AqCore.h"
#include "ExprParser.h"
#include <sstream>
#include <array>

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ExprParser::ParseExpr()
{
	Reset();

	while (mCurrent.Lexeme != "\n" && mPlace != mTokens.size())
	{
		auto exprOper = SeekOperator();

		// Parenthesis are special cases, so they need to handled separately
		if (Match("("))
		{
			// Push ( operator on the operator stack...
			ExpressionOperation operation = CreateBracketOp();
			mOperatorStack.push(operation);
		}
		else if (Match(")"))
		{
			// Reduce until we hit a '(' operator
			ReduceUntilLowerPrecedenceHits(eBracketPrec);

			// TODO: Expecting '(' operator, otherwise we have an error...
			// Error handling remains to be done
		}
		else if (exprOper)
		{
			// We either push the operator or reduce the operator stack
			// depending upon the precedence of the newly encountered operator...

			ExpressionOperation expr = *exprOper;

			// Unary if nothing exist inside the operand stack
			// Or if operators appear consecutively
			if (mOperandStack.empty())// || mOperatorStreak != 0)
			{
				expr.Precedence = static_cast<int32_t>(INT32_MAX);
				expr.OperandCount = 1;
			}

			ReduceUntilLowerPrecedenceHits(expr.Precedence);
			mOperatorStack.push(expr);
		}
		else
		{
			ParseNonOperator();

			// We have advanced already, so going straight back to the loop again
			continue;
		}

		Advance();
	}

	ReduceEntireOperatorStack();

	_STL_ASSERT(mOperandStack.size() == 1, "Could not parse expr!");

	return mOperandStack.top();
}

void AQUA_NAMESPACE::ExprParser::Reset()
{
	mCurrent = mTokens[0];
	mPlace = 0;

	mCtx = ParserContext();

	while (!mOperandStack.empty())
		mOperandStack.pop();

	while (!mOperatorStack.empty())
		mOperatorStack.pop();
}

bool AQUA_NAMESPACE::ExprParser::Expect(const std::string& expected)
{
	std::string Concatenation;

	Concatenation += mCurrent.Lexeme;

	int LineNo = mCurrent.LineNo;
	int CharOff = mCurrent.CharOff;

	// The expected string might spread out among multiple tokens
	while (Concatenation.size() < expected.size() && mPlace != mTokens.size())
	{
		Advance();
		Concatenation += mCurrent.Lexeme;
	}

	if (Concatenation != expected)
	{
		std::stringstream stream;
		stream << "Unexpected token at " << expected;

		ConstructError(stream.str(), LineNo, CharOff, ErrorType::eSyntaxError);

		return false;
	}

	return true;
}

bool AQUA_NAMESPACE::ExprParser::Match(const std::string& rhs)
{
	std::string Concatenation;

	size_t temp = mPlace;

	Token token = mTokens[mPlace];
	Concatenation += token.Lexeme;

	int LineNo = token.LineNo;
	int CharOff = token.CharOff;

	// The expected string might spread out among multiple tokens
	while (Concatenation.size() < rhs.size() && mPlace != mTokens.size())
	{
		token = mTokens[++mPlace];
		Concatenation += token.Lexeme;
	}

	mPlace = temp;

	return Concatenation == rhs;
}

void AQUA_NAMESPACE::ExprParser::Advance()
{
	mCurrent = mTokens[++mPlace];
}

bool AQUA_NAMESPACE::ExprParser::Reduce()
{
	if (mOperatorStack.empty())
		return true;

	auto& oper = mOperatorStack.top();

	if (oper.Lexeme == "(" || oper.Lexeme == ")")
	{
		mOperatorStack.pop();
		return false;
	}

	if (mOperandStack.size() < oper.OperandCount)
	{
		ConstructError("Invalid right hand side expression", mCurrent.LineNo, 
			mCurrent.CharOff, ErrorType::eSyntaxError);

		return false;
	}

	std::vector<NodeAST*> operands(oper.OperandCount);

	for (int32_t i = 0; i < oper.OperandCount; i++)
	{
		operands[i] = mOperandStack.top();
		mOperandStack.pop();
	}

	NodeAST* operation = CreateNode(oper.Lexeme, oper.OpType);
	operation->Children.insert(operation->Children.end(), operands.begin(), operands.end());

	mOperandStack.push(operation);
	mOperatorStack.pop();

	return true;
}

void AQUA_NAMESPACE::ExprParser::ReduceUntilLowerPrecedenceHits(int32_t level)
{
	if (mOperatorStack.empty())
		return;

	int32_t currLevel = mOperatorStack.top().Precedence;

	while (currLevel >= level)
	{
		Reduce();

		if (mOperatorStack.empty())
			break;

		currLevel = mOperatorStack.top().Precedence;
	}
}

void AQUA_NAMESPACE::ExprParser::ReduceEntireOperatorStack()
{
	while (!mOperatorStack.empty())
		Reduce();
}

void AQUA_NAMESPACE::ExprParser::ConstructError(const std::string& errorInfo, 
	int LineNo, int CharOff, ErrorType errorType)
{
	NodeError error{};
	error.Message = errorInfo;
	error.LineNumber = LineNo;
	error.CharOffset = CharOff;
	error.Type = errorType;

	mCtx.Errors.push(error);
}

void AQUA_NAMESPACE::ExprParser::ConstructErrorAndRetrieveLexer(const std::string& errorInfo,
	int LineNo, int CharOff, ErrorType errorType, size_t marker)
{
	mPlace = marker;
	mCurrent = mTokens[mPlace];

	ConstructError(errorInfo, LineNo, CharOff, errorType);
}

bool AQUA_NAMESPACE::ExprParser::ParseNumericLiteral()
{
	std::string NumericLiteral;

	auto CountChars = [&NumericLiteral](char found)->int
	{
		int count = 0;

		for (char c : NumericLiteral)
		{
			if (c == found)
				count++;
		}

		return count;
	};

	size_t temp = mPlace;
	Token curr = mTokens[++mPlace];

	NodeType literalType = NodeType::eIntLiteral;

	std::string result;

	if (!ContainFloatNumericLiterals(curr.Lexeme))
	{
		ConstructErrorAndRetrieveLexer("Expected numeric literal", 
			curr.LineNo, curr.CharOff, ErrorType::eSyntaxError, temp);
	}
	else
	{
		NumericLiteral += curr.Lexeme;

		temp = mPlace;
		curr = mTokens[++mPlace];

		if (curr.Lexeme == ".")
		{
			literalType = NodeType::eFloatLiteral;

			NumericLiteral += ".";

			temp = mPlace;
			curr = mTokens[++mPlace];

			if (!ContainFloatNumericLiterals(curr.Lexeme))
			{
				ConstructErrorAndRetrieveLexer("Expected numeric literal", 
					curr.LineNo, curr.CharOff, ErrorType::eSyntaxError, temp);

				mOperandStack.push(CreateNode(NumericLiteral, literalType));

				return false;
			}
			else
			{
				NumericLiteral += curr.Lexeme;

				temp = mPlace;
				curr = mTokens[++mPlace];
			}
		}

		int e_s = CountChars('e');
		int E_s = CountChars('E');

		if (e_s != 0 || E_s != 0)
		{
			// The exponents should be at the end
			if (NumericLiteral.back() != 'e' && NumericLiteral.back() != 'E')
			{
				ConstructErrorAndRetrieveLexer("Invalid exponent position",
					curr.LineNo, curr.CharOff, ErrorType::eSyntaxError, temp);

				mOperandStack.push(CreateNode(NumericLiteral, literalType));

				return false;
			}

			literalType = NodeType::eFloatLiteral;

			if (curr.Lexeme == "+")
			{
				temp = mPlace;
				curr = mTokens[++mPlace];
			}
			else if (curr.Lexeme == "-")
			{
				NumericLiteral += "-";

				temp = mPlace;
				curr = mTokens[++mPlace];
			}

			if (!ContainIntNumericLiterals(curr.Lexeme))
			{
				ConstructErrorAndRetrieveLexer("Invalid exponent literal",
					curr.LineNo, curr.CharOff, ErrorType::eSyntaxError, temp);

				mOperandStack.push(CreateNode(NumericLiteral, literalType));

				return false;
			}
			else
			{
				NumericLiteral += curr.Lexeme;

				temp = mPlace;
				mPlace++;
			}
		}
	}

	mOperandStack.push(CreateNode(NumericLiteral, literalType));

	return true;
}

bool AQUA_NAMESPACE::ExprParser::ContainFloatNumericLiterals(const std::string_view& lexeme)
{
	if (lexeme.empty()) return false;

	size_t i = 0;

	for (; i < lexeme.size(); ++i) 
	{
		if (!std::isdigit(lexeme[i]) &&
			lexeme[i] != 'e' &&
			lexeme[i] != 'E')

			return false;
	}

	return true;
}

bool AQUA_NAMESPACE::ExprParser::ContainIntNumericLiterals(const std::string_view& lexeme)
{
	if (lexeme.empty()) return false;

	size_t i = 0;

	for (; i < lexeme.size(); ++i)
	{
		if (!std::isdigit(lexeme[i]))
			return false;
	}

	return true;
}

const AQUA_NAMESPACE::ExpressionOperation* AQUA_NAMESPACE::ExprParser::SeekOperator()
{
	size_t scan = mPlace;
	Token token = mTokens[scan];

	if (mCurrent.Lexeme.size() > mMaxLength)
	{
		return nullptr;
	}

	std::string Concatenation;
	const ExpressionOperation* op = nullptr;

	while (true)
	{
		Concatenation += token.Lexeme;

		if (Concatenation.size() > mMaxLength)
			break;

		const ExpressionOperation* newOp = RetrieveOperator(Concatenation);

		if (newOp)
		{
			op = newOp;
			mPlace = scan;
		}

		if (scan + 1 == mTokens.size())
			break;

		token = mTokens[++scan];
	}

	mCurrent = mTokens[mPlace];

	return op;
}

const AQUA_NAMESPACE::ExpressionOperation* AQUA_NAMESPACE::ExprParser::
	RetrieveOperator(const std::string& Concatenation) const
{
	auto& operators = mOpsTable.at(Concatenation.size());

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


bool AQUA_NAMESPACE::ExprParser::IsValidToken(const Token& token, const std::vector<int(__cdecl*)(int)>& myTests)
{
	for (char c : token.Lexeme)
	{
		bool IsValid = std::any_of(myTests.begin(), myTests.end(), [c](int(__cdecl* test)(int))->bool
		{
			return test(c) != 0;
		});

		if (!IsValid)
		{
			return false;
		}
	}

	return true;
}

bool AQUA_NAMESPACE::ExprParser::IsValidIdentifier(const Token& token)
{
	std::vector<int(__cdecl*)(int)> myTests;
	myTests.push_back(isdigit);
	myTests.push_back(isalpha);
	myTests.push_back([](int c)->int { return c == '_'; });

	return IsValidToken(token, myTests);
}

void AQUA_NAMESPACE::ExprParser::ParseNonOperator()
{
	/*
	* We have encountered an identifier...
	* There are three cases
	*	1. A function call (which is always followed by '(' bracket)
	*	2. A float or int literal (all numerics in the string)
	*	3. A Reference to a scoped variable (when none of the condition above hold)
	*/

	// Storing the current identifier to a local variable
	Token token = mTokens[mPlace];
	std::string lexeme(token.Lexeme);

	bool IsValid = IsValidIdentifier(token);

	// Stepping over to the next token in advance to look for an open parenthesis
	Advance();

	if (Match("("))
	{
		// First case scenario, push the identifier as an operator with infinite precedence
		// Also, check if the identifier adheres to the grammar of the script language
		if (!IsValid)
		{
			ConstructError("Invalid Identifier: " + lexeme,
				token.LineNo, token.CharOff, ErrorType::eSyntaxError);
		}

		ExpressionOperation function = CreateFunctionOp(lexeme);
		ExpressionOperation bracket = CreateBracketOp();

		mOperatorStack.push(function);
		mOperatorStack.push(bracket);

		Advance();
	}
	else if (IsValid)
	{
		// Second case scenario, an identifier, i.e. variable reference
		mOperandStack.push(CreateNode(lexeme, NodeType::eIdentifier));
	}
	else
	{
		// Third case scenario, push the literal
		// on the operand stack with node type "NumericLiteral"
		bool Success = ParseNumericLiteral();

		if (!Success)
		{
			mOperandStack.top()->ErrorInfos.push_back(mCtx.Errors.top());
			mCtx.Errors.pop();
		}
	}
}

AQUA_NAMESPACE::NodeAST* AQUA_NAMESPACE::ExprParser::CreateNode(const std::string& name, NodeType type)
{
	return new NodeAST(name, type);
}
