#pragma once
#include "Execution/Graph.h"
#include "Execution/Draft.h"
#include "../Machine Learning/AutoDiff.h"

enum class NodeType
{
	eInvalid             = -1,
	eFloatLiteral        = 1,
	eIntLiteral          = 2,
	eStringLiteral       = 3,
	eOp                  = 4,
	eFunc                = 5,
	eArray               = 6,
};

template <typename _Char>
struct BasicOpInfo
{
	std::basic_string<_Char> Op;
	int32_t Precedence = 0;
	int32_t OperandCount = 0;
};

template <typename _Char>
using BasicExprNode = CalcNode<NodeType, double, int64_t, std::basic_string<_Char>, BasicOpInfo<_Char>>;

template <typename _Char>
using BasicExprNodeRef = Aqua::SharedRef<BasicExprNode<_Char>>;

template <typename _Char>
using BasicDerivFn = std::function<Aqua::Exec::BasicGraph<BasicExprNodeRef<_Char>>(const std::function<Aqua::Exec::NodeID()>&, const Aqua::Exec::BasicGraph<BasicExprNodeRef<_Char>>&, Aqua::Exec::NodeID, Aqua::Exec::NodeID)>;

template <typename _Char>
using BasicIdxMapType = std::unordered_map<std::basic_string<_Char>, std::tuple<BasicOpInfo<_Char>, BasicDerivFn<_Char>>>;

using IdxMapType = BasicIdxMapType<char>;
using OpInfo = BasicOpInfo<char>;
using ExprNode = BasicExprNode<char>;
using ExprNodeRef = BasicExprNodeRef<char>;

template <typename _Char>
bool operator==(const BasicExprNode<_Char>& first, const BasicExprNode<_Char>& second) { return first.mInfo == second.mInfo && first.mVar == second.mVar; }

template <typename _Char>
bool operator!=(const BasicExprNode<_Char>& first, const BasicExprNode<_Char>& second) { return !(first == second); }

struct IRCode
{
	Aqua::Exec::BasicGraph<ExprNodeRef> IR;
	//std::vector<FuncDef> Functions;
};

enum class NetType
{
	eInvalid             = 0,
	eNeuralNetwork       = 1,
	eCNN                 = 2,
	eRNN                 = 3,
};

