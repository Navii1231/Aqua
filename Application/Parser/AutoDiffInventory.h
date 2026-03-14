#pragma once
#include "config.h"
#include "ExprParser.h"

// keep the autodiff inventory alive for correct results
template <typename _Char>
class BasicAutoDiffInventory
{
public:
	using MyChar = _Char;
	using MyString = std::basic_string<_Char>;
	using MyExprNode = BasicExprNode<_Char>;
	using MyExprNodeRef = BasicExprNodeRef<_Char>;
	using MyExprGraph = Aqua::Exec::BasicGraph<MyExprNodeRef>;
	using MyRNG = std::function<Aqua::Exec::NodeID()>;
	using MyFnMap = std::unordered_map<MyString, MyExprGraph>;
	using MyDerivFn = BasicDerivFn<_Char>;
	using MyIdxMap = BasicIdxMapType<_Char>;
	using MyOpInfo = BasicOpInfo<_Char>;

public:
	BasicAutoDiffInventory();
	~BasicAutoDiffInventory() = default;

	void AddFn(const std::string& fn, const std::string& derivative, const std::vector<std::string>& inputVars = {});

	const MyFnMap& GetFnMap() const { return mFnMap; }
	const auto& GetFnDeriv(const MyString& fn) const { return mFnMap.at(fn); }

	const MyIdxMap& GetIdxMap() const { return mIdxMap; }
	const auto& operator[](const MyString& op) const { return mIdxMap.at(op); }

	std::unordered_map<MyChar, std::vector<MyOpInfo>> GetOpMap() const
	{
		std::unordered_map<MyChar, std::vector<MyOpInfo>> opMap{};

		for (const auto& [key, val] : mIdxMap)
		{
			const auto& [opInfos, fn] = val;
			opMap[*key.begin()].emplace_back(opInfos);
		}

		return opMap;
	}

private:
	BasicAutoDiffInventory(const BasicAutoDiffInventory&) = delete;
	BasicAutoDiffInventory& operator=(const BasicAutoDiffInventory&) = delete;

private:
	MyFnMap mFnMap;
	MyIdxMap mIdxMap;

private:
	void GenerateIdxMap();
	void GenerateFnMap();
};

using AutoDiffInventory = BasicAutoDiffInventory<char>;

template <typename _Char>
BasicAutoDiffInventory<_Char>::BasicAutoDiffInventory()
{
	GenerateIdxMap();
	GenerateFnMap();
}

template <typename _Char>
void BasicAutoDiffInventory<_Char>::AddFn(const std::string& fn, const std::string& derivative, const std::vector<std::string>& inputVars /*= {}*/)
{
	BasicExprParser<_Char> parser{};

	Aqua::Exec::NodeID val = 0;
	for (const auto& var : inputVars)
	{
		parser[var] = ++val;
	}

	parser.SetOpMap(GetOpMap());

	auto parseFn = [this, &parser](const MyString& str)
		{
			parser.SetString(str);
			return *parser.Parse();
		};

	mFnMap[fn] = parseFn(derivative);
}

template <typename _Char>
void BasicAutoDiffInventory<_Char>::GenerateIdxMap()
{
	mIdxMap["+"] = { {"+", 1}, [this](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
	{
			// df = du + dv

			MyExprGraph deriv{};

			auto rdm = idGen();
			deriv.Nodes[rdm] = Aqua::MakeRef<ExprNode>(rdm);
			deriv[rdm].mInfo = NodeType::eIntLiteral;
			deriv[rdm].mVar = 1;

			deriv.InputNodes.emplace_back(rdm);
			deriv.OutputNodes.emplace_back(rdm);

			return deriv;
		} };

	mIdxMap["-"] = { {"-", 1}, [this](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
		{
			// df = du - dv

			MyExprGraph deriv{};

			auto rdm = idGen();

			deriv.Nodes[rdm] = Aqua::MakeRef<ExprNode>(rdm);
			deriv[rdm].mInfo = NodeType::eIntLiteral;
			deriv[rdm].mVar = childID == 1 || graph[nodeID].mChildren.size() == 1 ? -1 : 1;

			deriv.InputNodes.emplace_back(rdm);
			deriv.OutputNodes.emplace_back(rdm);

			return deriv;
		} };

	mIdxMap["*"] = { {"*", 2}, [this](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
		{
			// f = u * v
			// df / f = du / u + dv / v

			MyExprGraph deriv{};

			Aqua::Exec::NodeID otherID = 1 - childID;
			auto otherNodeID = GetChildren(graph[nodeID])[otherID];
			auto& otherNode = graph.Nodes.at(otherNodeID);

			CloneExByRef(deriv, graph, otherNodeID);
			deriv.OutputNodes.emplace_back(otherNodeID);

			return deriv;
		} };

	mIdxMap["/"] = { {"/", 2}, [this](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
		{
			// f = u / v
			// df / f = du / u - dv / v

			MyExprGraph deriv{};

			Aqua::Exec::NodeID otherID = 1 - childID;
			auto& children = GetChildren(graph[nodeID]);

			auto divID = idGen();

			deriv.Nodes[divID] = Aqua::MakeRef<ExprNode>(divID);
			deriv.OutputNodes.emplace_back(divID);

			deriv[divID].mNodeID = 0;
			deriv[divID].mInfo = NodeType::eOp;
			deriv[divID].mVar = std::get<OpInfo>(mIdxMap["/"]);

			if (childID == 0)
			{
				auto floatID = idGen();

				deriv.Nodes[floatID] = Aqua::MakeRef<ExprNode>(floatID);
				deriv[floatID].mNodeID = floatID;
				deriv[floatID].mInfo = NodeType::eFloatLiteral;
				deriv[floatID].mVar = 1.0;

				deriv[divID].mChildren.emplace_back(floatID);
				deriv[divID].mChildren.emplace_back(std::get<0>(CloneExByRef(deriv, graph, children[1])));

				// emplace the children and returning the sub-tree
				return deriv;
			}

			auto negationID = idGen();
			auto minusOp = std::get<OpInfo>(mIdxMap["-"]);
			minusOp.OperandCount = 1;

			deriv.Nodes[negationID] = Aqua::MakeRef<ExprNode>(negationID);
			deriv[negationID].mInfo = NodeType::eOp;
			deriv[negationID].mVar = minusOp;

			auto [childZeroID, childZeroIDState] = CloneExByRef(deriv, graph, children[0]);

			auto powerOpID = idGen();

			deriv.Nodes[powerOpID] = Aqua::MakeRef<ExprNode>(powerOpID);
			deriv[powerOpID].mInfo = NodeType::eOp;
			deriv[powerOpID].mVar = std::get<OpInfo>(mIdxMap["^"]);

			auto [baseID, baseIDState] = CloneExByRef(deriv, graph, children[1]);

			auto expID = idGen();

			deriv.Nodes[expID] = Aqua::MakeRef<ExprNode>(expID);
			deriv[expID].mInfo = NodeType::eIntLiteral;
			deriv[expID].mVar = 2;

			deriv[negationID].mChildren.emplace_back(divID);
			deriv[divID].mChildren.emplace_back(childZeroID);
			deriv[divID].mChildren.emplace_back(powerOpID);
			deriv[powerOpID].mChildren.emplace_back(baseID);
			deriv[powerOpID].mChildren.emplace_back(expID);

			// emplace the children and returning the sub-tree
			return deriv;
		} };

	mIdxMap["^"] = { {"^", 3}, [this](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
	{
		MyExprGraph deriv{};

		auto& children = GetChildren(graph[nodeID]);

		auto mulID = idGen();

		deriv.Nodes[mulID] = Aqua::MakeRef<ExprNode>(mulID);
		deriv[mulID].mInfo = NodeType::eOp;
		deriv[mulID].mVar = std::get<OpInfo>(mIdxMap["*"]);

		deriv.OutputNodes.emplace_back(mulID);

		// CASE 1: derivative wrt u
		// v * u^(v - 1)
		if (childID == 0)
		{
			auto [vID, vIDState] = CloneExByRef(deriv, graph, children[1]);

			auto powID = idGen();
			deriv.Nodes[powID] = Aqua::MakeRef<ExprNode>(powID);
			deriv[powID].mInfo = NodeType::eOp;
			deriv[powID].mVar = std::get<OpInfo>(mIdxMap["^"]);

			auto [uID, uIDState] = CloneExByRef(deriv, graph, children[0]);

			auto minusID = idGen();
			deriv.Nodes[minusID] = Aqua::MakeRef<ExprNode>(minusID);
			deriv[minusID].mInfo = NodeType::eOp;
			deriv[minusID].mVar = std::get<OpInfo>(mIdxMap["-"]);

			auto [vCloneID, vCloneState] = CloneExByRef(deriv, graph, children[1]);

			auto oneID = idGen();
			deriv.Nodes[oneID] = Aqua::MakeRef<ExprNode>(oneID);
			deriv[oneID].mInfo = NodeType::eIntLiteral;
			deriv[oneID].mVar = 1;

			deriv[mulID].mChildren.emplace_back(vID);
			deriv[mulID].mChildren.emplace_back(powID);

			deriv[powID].mChildren.emplace_back(uID);
			deriv[powID].mChildren.emplace_back(minusID);

			deriv[minusID].mChildren.emplace_back(vCloneID);
			deriv[minusID].mChildren.emplace_back(oneID);

			return deriv;
		}

		// CASE 2: derivative wrt v
		// ln(u) * u^v

		auto lnID = idGen();
		deriv.Nodes[lnID] = Aqua::MakeRef<ExprNode>(lnID);
		deriv[lnID].mInfo = NodeType::eFunc;
		deriv[lnID].mVar = OpInfo("ln", 1);

		auto [uID, uIDState] = CloneExByRef(deriv, graph, children[0]);

		auto powID = idGen();
		deriv.Nodes[powID] = Aqua::MakeRef<ExprNode>(powID);
		deriv[powID].mInfo = NodeType::eOp;
		deriv[powID].mVar = std::get<OpInfo>(mIdxMap["^"]);

		auto [uCloneID, uCloneState] = CloneExByRef(deriv, graph, children[0]);
		auto [vCloneID, vCloneState] = CloneExByRef(deriv, graph, children[1]);

		deriv[mulID].mChildren.emplace_back(lnID);
		deriv[mulID].mChildren.emplace_back(powID);

		deriv[lnID].mChildren.emplace_back(uID);

		deriv[powID].mChildren.emplace_back(uCloneID);
		deriv[powID].mChildren.emplace_back(vCloneID);

		return deriv;
	} };

	mIdxMap["("] = { {"(", -1, 1}, [](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
		{
			return {};
		} };

	mIdxMap[")"] = { {")", -2, 1}, [](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
		{
			return {};
		} };

	mIdxMap["["] = { {"[", -3, 1}, [](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
		{
			return {};
		} };

	mIdxMap["]"] = { {"]", -4, 1}, [](const MyRNG& idGen, const MyExprGraph& graph, Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->MyExprGraph
		{
			return {};
		} };
}

template <typename _Char>
void BasicAutoDiffInventory<_Char>::GenerateFnMap()
{
	BasicExprParser<_Char> parser{};
	parser.SetOpMap(GetOpMap());

	auto parseFn = [this, &parser](const std::string& str)
		{
			parser.SetString(str);
			return *parser.Parse();
		};

	mFnMap["sin"] = parseFn("cos(x)");
	mFnMap["cos"] = parseFn("sin(x)");
	mFnMap["tan"] = parseFn("sec(x) * sec(x)");
	mFnMap["asin"] = parseFn("1.0 / ((1.0 - x ^ 2.0) ^ (1.0 / 2.0))");
	mFnMap["acos"] = parseFn("-1.0 / ((1.0 - x ^ 2.0) ^ (1.0 / 2.0))");
	mFnMap["atan"] = parseFn("1.0 / (1.0 + x ^ 2.0)");
	mFnMap["sinh"] = parseFn("cosh(x)");
	mFnMap["cosh"] = parseFn("sinh(x)");
	mFnMap["tanh"] = parseFn("sech(x) * sech(x)");
	mFnMap["ln"] = parseFn("1.0 / x");
	mFnMap["exp"] = parseFn("exp(x)");
}

