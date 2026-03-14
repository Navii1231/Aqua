#pragma once
#include "Config.h"
#include "Utils/Lexer.h"

// GLSL requires a lot of boilerplate code, I need to simplify the language
using ExprGraph = Aqua::Exec::BasicGraph<ExprNodeRef>;

template <typename _Char>
struct BasicExprError
{
	std::basic_string<_Char> Info{};

	operator bool() const { return !Info.empty(); }
};

// parser for mathematical exprs
// using shift reduce parsing technique
// negative odd precedence represents opening brace
// negative even precedence represents closing brace

// only accepts infix or unary notations
template <typename _Char>
class BasicExprParser
{
public:
	using MyLexer = Aqua::BasicLexer<_Char>;
	using MyToken = typename MyLexer::MyToken;
	using MyChar = typename MyLexer::MyChar;
	using MyString = typename MyLexer::MyString;
	using MyStringView = typename MyLexer::MyStringView;
	using MyOpInfo = BasicOpInfo<_Char>;
	using MyOpMap = std::unordered_map<MyChar, std::vector<MyOpInfo>>;
	using MyExprError = BasicExprError<MyChar>;
	using MyExprNode = BasicExprNode<_Char>;
	using MyExprNodeRef = BasicExprNodeRef<_Char>;
	using MyExprGraph = Aqua::Exec::BasicGraph<MyExprNodeRef>;
	using MyAutoDiff = AutoDiff<MyExprNode>;
	using MyNodeID = ExprNode::MyIDType;
	using MyRNGFn = std::function<MyNodeID()>;

public:
	BasicExprParser() : mRNG(Aqua::Exec::UniqueIDGen()) { }
	BasicExprParser(MyRNGFn rng) : mRNG(rng) {}
	~BasicExprParser() = default;

	void SetIDGen(MyRNGFn rng) { mRNG = rng; }
	void SetString(const MyStringView& expr) { mLexer.SetString(expr); }
	void SetOpMap(const MyOpMap& opMap) { mOpMap = opMap; }

	Aqua::Exec::NodeID& operator[](const MyString& id) { return mInputVariables[id]; }
	Aqua::Exec::NodeID operator[](const MyString& id) const { return mInputVariables.at(id); }

	void ClearInputVariables() { mInputVariables.clear(); }

	// takes a string and returns an AST or you might say an intermediate representation
	std::expected<MyExprGraph, MyExprError> Parse()
	{
		Clear();

		// should be set by the user...
		mLexer.SetWhiteSpacesAndDelimiters(" \t\r", "\n+-*/^()[]");

		while (!mLexer.HasConsumed())
		{
			mLexer++;
			auto opInfo = EmplaceIntoBin();

			if (!opInfo)
				return std::unexpected(opInfo.error());
		}

		auto err = ReduceEverything();

		if (!err)
			return std::unexpected(err.error());

		mGraph.OutputNodes.emplace_back(mVarStack[0]->mNodeID);

		for (auto& [id, node] : mGraph.Nodes)
		{
			std::reverse(node->mChildren.begin(), node->mChildren.end());
		}

		// turn this into a graph
		return mGraph;
	}

private:
	MyLexer mLexer;

	std::vector<MyExprNodeRef> mOpStack;
	std::vector<MyExprNodeRef> mVarStack;

	MyExprGraph mGraph;
	MyRNGFn mRNG;

	bool mLastOneOp = true;

	// if the given token is not an operator, it is either a variable or a function or an invalid token
	MyOpMap mOpMap;

	// input variables
	std::unordered_map<MyString, Aqua::Exec::NodeID> mInputVariables;

private:
	Aqua::Exec::NodeID GetRandom() const { return mRNG(); }

	void Clear()
	{
		mOpStack.clear();
		mVarStack.clear();
		mGraph = {};
		mLastOneOp = true;

		mLexer.Reset();
	}

private:

	// from broader to more "granularity" level
	// operand op operand
	std::expected<bool, MyExprError> ReduceAnOp(int count)
	{
		if (count > mVarStack.size())
			return std::unexpected(MyExprError("bad syntax"));

		auto op = mOpStack.back();
		mOpStack.pop_back();

		for (; count > 0; count--)
		{
			op->mChildren.push_back(mVarStack.back()->mNodeID);
			mVarStack.pop_back();
		}

		mVarStack.push_back(op);

		return true;
	}

	std::expected<bool, MyExprError> ReduceOps(int count)
	{
		for (; count > 0; count--)
		{
			auto err = ReduceAnOp(std::get<OpInfo>(mOpStack.back()->mVar).OperandCount);

			if (!err)
				return std::unexpected(err.error());
		}

		return true;
	}

	std::expected<bool, MyExprError> EmplaceBackOp(const OpInfo& info)
	{
		int count = 0;

		for (auto rBegin = mOpStack.rbegin(); rBegin != mOpStack.rend(); rBegin++)
		{
			if (std::get<OpInfo>((*rBegin)->mVar).Precedence < info.Precedence)
				break;

			count++;
		}

		auto err = ReduceOps(count);
		if (!err)
			return std::unexpected(err.error());

		ExprNodeRef op = MakeNode();
		op->mInfo = NodeType::eOp;
		op->mVar = info;

		mOpStack.push_back(op);

		return true;
	}

	std::expected<bool, MyExprError> ReduceUntilCloseBraceHits(int bracePrec)
	{
		if ((-bracePrec) % 2 == 0)
			return true;

		int count = 0;

		for (auto rbegin = mOpStack.rbegin(); rbegin != mOpStack.rend(); rbegin++)
		{
			if (std::get<OpInfo>((*rbegin)->mVar).Precedence == bracePrec)
				break;

			count++;
		}

		mGraph.Nodes.erase(mOpStack.back()->mNodeID);
		mOpStack.pop_back();

		if (count == 0)
		{
			// () expression
			mGraph.Nodes.erase(mOpStack.back()->mNodeID);
			mOpStack.pop_back();
			return true;
		}

		auto success = ReduceOps(count - 1);

		mGraph.Nodes.erase(mOpStack.back()->mNodeID);
		mOpStack.pop_back();

		if (!success)
			return std::unexpected(success.error());

		if (mOpStack.back()->mInfo == NodeType::eFunc)
		{
			// function call
			mOpStack.back()->mChildren.push_back(mVarStack.back()->mNodeID);
			mVarStack.back() = mOpStack.back();

			mOpStack.pop_back();
		}
		else if (mOpStack.back()->mInfo == NodeType::eArray)
		{
			// array, could be an input variable
			auto childID = mVarStack.back()->mNodeID;

			mOpStack.back()->mChildren.push_back(childID);
			mVarStack.back() = mOpStack.back();

			mOpStack.pop_back();

			// finding out if this is an input variable...

			const auto& opInfo = std::get<OpInfo>(mVarStack.back()->mVar);

			if (mInputVariables.find(opInfo.Op) != mInputVariables.end())
			{
				auto childRef = mGraph.Nodes[childID];

				if (childRef->mInfo == NodeType::eIntLiteral)
					mGraph.InputNodes.push_back(mVarStack.back()->mNodeID);
			}
		}

		return success;
	}

	std::expected<bool, MyExprError> ReduceEverything()
	{
		size_t count = mOpStack.size();
		return ReduceOps((int) count);
	}

	std::expected<bool, MyExprError> EmplaceIntoBin()
	{
		if (IsAnOp(*mLexer))
		{
			auto currInfo = mOpMap[*mLexer->begin()][0];
			bool lastOneWasOp = mLastOneOp;

			mLastOneOp = true;

			if(currInfo.Precedence < 0 && (-currInfo.Precedence) % 2 == 0)
				mLastOneOp = false;

			if (currInfo.OperandCount == 0)
			{
				currInfo.OperandCount = 1 + !lastOneWasOp;

				if (!mLastOneOp)
					currInfo.Precedence = std::numeric_limits<int32_t>::max();
			}

			if (IsItABrace(currInfo))
			{
				ExprNodeRef op = MakeNode();
				op->mInfo = NodeType::eOp;
				op->mVar = currInfo;

				mOpStack.push_back(op);
				return ReduceUntilCloseBraceHits(currInfo.Precedence + 1);
			}
			
			return EmplaceBackOp(currInfo);
		}

		mLastOneOp = false;
		auto varName = *mLexer;
		auto nextToken = mLexer + 1;

		if (nextToken == "(")
		{
			ExprNodeRef op = MakeNode();
			op->mInfo = NodeType::eFunc;
			op->mVar = OpInfo(varName, 0);

			mOpStack.push_back(op);

			ExprNodeRef brace = MakeNode();
			brace->mInfo = NodeType::eOp;
			brace->mVar = mOpMap['('][0];

			mOpStack.push_back(brace);
			mLastOneOp = true;

			mLexer++;

			return true;
		}
		if (nextToken == "[")
		{
			ExprNodeRef op = MakeNode();
			op->mInfo = NodeType::eArray;
			op->mVar = OpInfo(varName, 0);

			mOpStack.push_back(op);

			ExprNodeRef brace = MakeNode();
			brace->mInfo = NodeType::eOp;
			brace->mVar = mOpMap['['][0];

			mOpStack.push_back(brace);
			mLastOneOp = true;

			mLexer++;

			return true;
		}

		ExprNodeRef var = MakeNode();
		
		auto err = ResolveVariable();

		if (!err)
			return std::unexpected(err.error());

		auto [type, literal] = *err;

		var->mInfo = type;
		var->mVar = literal;

		mVarStack.push_back(var);

		if (type == NodeType::eStringLiteral)
		{
			const auto& strLit = std::get<std::string>(literal);

			if(mInputVariables.empty() || mInputVariables.find(strLit) != mInputVariables.end())
				mGraph.InputNodes.emplace_back(var->mNodeID);
		}

		return false;
	}

	bool IsItABrace(const OpInfo& info)
	{
		if (info.Precedence < 0)
			return true;

		return false;
	}

	// an operator
	bool IsAnOp(const MyToken& op)
	{ 
		if (mOpMap.find(op[0]) == mOpMap.end())
			return false;

		for (const auto& str : mOpMap[op[0]])
		{
			size_t idx = 0;
			std::string concat{};

			while (concat.size() < str.Op.size())
			{
				concat += (mLexer + idx);
				idx++;
			}

			if (str.Op == concat)
				return true;
		}

		return false;
	}

	std::expected<std::tuple<NodeType, ExprNode::MyVariant>, MyExprError> ResolveVariable()
	{
		if (!std::isdigit((*mLexer)[0]))
		{
			// string literal
			NodeType type = NodeType::eStringLiteral;

			for (auto c : *mLexer)
			{
				if (std::isdigit(c) || std::isalpha(c) || c == '_')
					continue;

				return std::unexpected(MyExprError("bad string literal"));
			}

			return std::tuple<NodeType, ExprNode::MyVariant>(type, *mLexer);
		}

		// float or int literal
		int floating = 0;

		for (auto c : *mLexer)
		{
			if (std::isdigit(c) || c == '.' || c == 'f')
			{
				floating += (c == '.' || c == 'f');
				continue;
			}

			return std::unexpected(MyExprError("bad numerical literal"));
		}

		if (floating == 0)
			return std::tuple<NodeType, ExprNode::MyVariant>(NodeType::eIntLiteral, std::stoi(*mLexer));
		
		return std::tuple<NodeType, ExprNode::MyVariant>(NodeType::eFloatLiteral, std::stof(*mLexer));
	}

	ExprNodeRef MakeNode()
	{
		auto node = Aqua::MakeRef<ExprNode>();
		node->mNodeID = GetRandom();
		node->mCalType = CalcNodeType::eFunction;

		mGraph.Nodes[node->mNodeID] = node;

		return node;
	}
};

using ExprParser = BasicExprParser<char>;

