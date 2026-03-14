#pragma once
#include "Config.h"
#include "Utils/Lexer.h"
#include "ExprParser.h"
#include "../Machine Learning/AutoDiff.h"
#include "AutoDiffInventory.h"

using NeuralNetworkIR = Aqua::Exec::BasicGraph<ExprGraph>;

// GLSL requires a lot of boilerplate code, I need to simplify the language
struct IRContext
{
	NeuralNetworkIR IRNodes;
	std::vector<ExprGraph> IRBridges;

	ExprGraph IRGraph{};
	ExprGraph IRDiffGraph{};

	bool Forward = true;
	NetType Type = NetType::eInvalid;

	// other stuff
};

// activation and edge function
class IRNeuralDraft : public Aqua::Exec::Draft<std::string, std::string, Aqua::Exec::NodeID>
{
public:
	IRNeuralDraft()
	{
		mAutoDiffInventory = Aqua::MakeRef<AutoDiffInventory>();
		mAutoDiffInventory->AddFn("sig", "sig(x) * (1.0 - sig(x))", { "x" });
	}

	~IRNeuralDraft() = default;

	// the input and outputs can figured out
	IRContext Construct(const Aqua::Exec::Wavefront& probes)
	{
		IRContext ctx{};
		// the probes have to be figured out by looking at the network

		Aqua::Exec::Wavefront parameters{};
		auto opMap = mAutoDiffInventory->GetOpMap();
		auto localRNG = [this]() { return mRNG(); };

		auto processNode = [this](Aqua::Exec::BasicGraph<ExprNodeRef>* dist, const Aqua::Exec::BasicGraph<ExprNodeRef>& src, Aqua::Exec::NodeID srcID)->std::pair<Aqua::Exec::NodeID, Aqua::Exec::TraversalState>
			{
				if (dist->Nodes.find(srcID) != dist->Nodes.end())
					return { srcID, Aqua::Exec::TraversalState::eSkip };

				dist->Nodes[srcID] = Aqua::MakeRef(*src.Nodes.at(srcID));
				return { srcID, Aqua::Exec::TraversalState::eSuccess };
			};

		std::mutex mtx{};

		ctx.IRNodes.OutputNodes = probes;
		ctx.IRNodes.InputNodes = *_ConstructEx<ExprGraph>(probes, true, [this, &localRNG, &opMap, &mtx, &ctx](Aqua::Exec::NodeID id, const std::string& code)->ExprGraph&
			{
				{
					std::unique_lock locker(mtx);

					if (ctx.IRNodes.Nodes.find(id) != ctx.IRNodes.Nodes.end())
						return ctx.IRNodes.Nodes[id];
				}

				ExprParser parser(localRNG);
				parser.SetString(code);
				parser.SetOpMap(opMap);
				parser["in"] = 1;

				// TODO: always going to have one output
				// you need a check here
				std::unique_lock locker(mtx);
				auto nodeErr = parser.Parse();
				ctx.IRNodes.Nodes[id] = *nodeErr;
				return ctx.IRNodes.Nodes[id];
			}, [this, &ctx, &localRNG, &opMap, &parameters, &processNode, &mtx](const NodeInfo& from, const NodeInfo& to, const std::string& depCode, Aqua::Exec::NodeID inputSlot)
				{
					ExprParser parser(localRNG);
					parser.SetString(depCode);
					parser.SetOpMap(opMap);
					parser["in"] = 1;

					ExprGraph connection;

					std::unique_lock locker(mtx);
					connection = *parser.Parse();
					// The input connects to the output of "from"
					// The output connects to the input of "to"
					// id's must be regenerated again

					auto& fromExpr = ctx.IRNodes.Nodes[from.ID];
					auto& toExpr = ctx.IRNodes.Nodes[to.ID];
					
					// after the connection inputs and outputs are coupled
					// and are integrated into toExpr, we'll add the nodes 
					// to forwardExpr

					auto [fromID, fromState] = Aqua::Exec::CloneEx(ctx.IRGraph.Nodes, fromExpr.Nodes, *fromExpr.OutputNodes.begin(), std::bind(processNode, &ctx.IRGraph, fromExpr, std::placeholders::_1), GetExprNodeChildren<ExprNodeRef>);

					auto [toID, toState] = Aqua::Exec::CloneEx(ctx.IRGraph.Nodes, toExpr.Nodes, *toExpr.OutputNodes.begin(), std::bind(processNode, &ctx.IRGraph, toExpr, std::placeholders::_1), GetExprNodeChildren<ExprNodeRef>);

					auto [conID, conState] = Aqua::Exec::CloneEx(ctx.IRGraph.Nodes, connection.Nodes, *connection.OutputNodes.begin(), std::bind(processNode, &ctx.IRGraph, connection, std::placeholders::_1), GetExprNodeChildren<ExprNodeRef>);

					// inserting the correct child node
					// bit of a patchwork
					// TODO: find the proper input slot...
					ctx.IRGraph.Nodes[toExpr.InputNodes[inputSlot]] = ctx.IRGraph.Nodes[conID];

					ctx.IRGraph.Nodes.erase(conID);
					ctx.IRGraph.Nodes[toExpr.InputNodes[inputSlot]]->mNodeID = toExpr.InputNodes[inputSlot];

					// emplacing the bridge to its input slot
					// making the input slot a + node for seamless stitching
					ctx.IRGraph.Nodes[connection.InputNodes.front()]->mInfo = NodeType::eOp;
					ctx.IRGraph.Nodes[connection.InputNodes.front()]->mVar = std::get<OpInfo>(mAutoDiffInventory->GetIdxMap().at("+"));

					ctx.IRGraph.Nodes[connection.InputNodes.front()]->mChildren.clear();
					ctx.IRGraph.Nodes[connection.InputNodes.front()]->mChildren.push_back(fromID);

					// traverse through all the graph and store the parameters ids
					for (auto [ID, node] : connection.Nodes)
					{
						if (node->mInfo == NodeType::eArray)
						{
							const auto& varName = std::get<OpInfo>(node->mVar).Op;
							const auto& children = GetExprNodeChildren(node);

							if (varName == "p" && children.size() == 1)
							{
								if (connection.Nodes[*children.begin()]->mInfo == NodeType::eIntLiteral)
								{
									parameters.push_back(ID);
								}
							}
						}
					}

					ctx.IRBridges.push_back(connection);
				}, Aqua::Exec::GetHardwareConcurrency());

		for (auto ID : ctx.IRNodes.OutputNodes)
		{
			for (auto output : ctx.IRNodes.Nodes[ID].OutputNodes)
			{
				_STL_ASSERT(ctx.IRGraph.Nodes.find(output) != ctx.IRGraph.Nodes.end(), "node doesn't exist");
				ctx.IRGraph.OutputNodes.push_back(output);
			}
		}

		for (auto ID : ctx.IRNodes.InputNodes)
		{
			for (auto _input : ctx.IRNodes.Nodes[ID].InputNodes)
			{
				_STL_ASSERT(ctx.IRGraph.Nodes.find(_input) != ctx.IRGraph.Nodes.end(), "node doesn't exist");
				ctx.IRGraph.InputNodes.push_back(_input);
			}
		}

		ExprParser::MyAutoDiff autoDiff{};
		autoDiff.SetExpr(ctx.IRGraph);
		autoDiff.SetIDGen([this]() { return mRNG(); });
		autoDiff.SetDerivFn([this, &ctx](Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)
			{
				if (ctx.IRGraph[nodeID].mInfo == NodeType::eOp)
				{
					ExprNodeRef ref = Aqua::MakeRef<ExprNode>();
					const auto& opInfo = std::get<OpInfo>(ctx.IRGraph[nodeID].mVar);

					const auto& [info, fn] = (*mAutoDiffInventory)[opInfo.Op];

					return fn([this]() { return mRNG(); }, ctx.IRGraph, nodeID, childID);
				}

				// we are left with functions or variables
				// for this we need a map to get the corresponding 
				// derivative of a function. The only difficult part
				// is getting the operators right.

				auto cloned = CloneByValue(mRNG, mAutoDiffInventory->GetFnDeriv(std::get<OpInfo>(ctx.IRGraph[nodeID].mVar).Op));

				CloneExByRef(cloned, ctx.IRGraph, nodeID);

				cloned.Nodes[cloned.InputNodes.front()] = cloned.Nodes[nodeID];
				cloned.Nodes[cloned.InputNodes.front()]->SetNodeID(cloned.InputNodes.front());
				cloned.Nodes.erase(nodeID);

				return cloned;	
			});

		autoDiff.SetInputVariables(parameters);
		ctx.IRDiffGraph = autoDiff.Apply();

		ctx.Type = NetType::eNeuralNetwork;

		return ctx;
	}

	void ResetRNG() { mRNG.Flush(); }
	
private:
	Aqua::Exec::UniqueIDGen mRNG;
	Aqua::SharedRef<AutoDiffInventory> mAutoDiffInventory;
};

void TestNeuralDraft()
{
	IRNeuralDraft smallDraft{};
	smallDraft[0] = "in[0]";
	smallDraft[1] = "in[0]";
	smallDraft[2] = "sig(in[0] + in[1])";
	smallDraft[3] = "sig(in[0] + in[1])";

	smallDraft.Connect(0, 2, "p[0] * in + p[1]", 0);
	smallDraft.Connect(1, 2, "p[0] * in + p[1]", 1);
	smallDraft.Connect(0, 3, "p[0] * in + p[1]", 0);
	smallDraft.Connect(1, 3, "p[0] * in + p[1]", 1);

	//auto smallCtx = smallDraft.Construct({ 2, 3 });

	IRNeuralDraft neuralDraft{};

	neuralDraft[0] = "in[0]";
	neuralDraft[1] = "in[0]";
	neuralDraft[2] = "in[0]";
	neuralDraft[3] = "in[0]";

	neuralDraft[4] = "sig(in[0] + in[1] + in[2] + in[3])";
	neuralDraft[5] = "sig(in[0] + in[1] + in[2] + in[3])";
	neuralDraft[6] = "sig(in[0] + in[1] + in[2] + in[3])";
	neuralDraft[7] = "sig(in[0] + in[1] + in[2] + in[3])";
	neuralDraft[8] = "sig(in[0] + in[1] + in[2] + in[3])";
	neuralDraft[9] = "sig(in[0] + in[1] + in[2] + in[3])";

	neuralDraft[10] = "sig(in[0] + in[1] + in[2] + in[3] + in[4] + in[5])";
	neuralDraft[11] = "sig(in[0] + in[1] + in[2] + in[3] + in[4] + in[5])";
	neuralDraft[12] = "sig(in[0] + in[1] + in[2] + in[3] + in[4] + in[5])";
	neuralDraft[13] = "sig(in[0] + in[1] + in[2] + in[3] + in[4] + in[5])";
	neuralDraft[14] = "sig(in[0] + in[1] + in[2] + in[3] + in[4] + in[5])";
	neuralDraft[15] = "sig(in[0] + in[1] + in[2] + in[3] + in[4] + in[5])";

	neuralDraft[16] = "sig(in[0] + in[1] + in[2] + in[3] + in[4] + in[5])";
	neuralDraft[17] = "sig(in[0] + in[1] + in[2] + in[3] + in[4] + in[5])";

	/*
	* AST for above maths expression (in yaml)
	* +:
	*	in[0]
	*	in[1]
	*/

	/*
	* AST for above maths expression (in yaml)
	* +:
	*	*:
	*		p[0]
	*		in
	*	p[1]
	*/

	// These ASTs are comparable and this allows us to perform our main trick
	// 'in' variable is the output of the 'from' network
	// the out variable is the input of the 'in' network

	neuralDraft.Connect(0, 4, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(0, 5, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(0, 6, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(0, 7, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(0, 8, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(0, 9, "p[0] * in + p[1]", 0);

	neuralDraft.Connect(1, 4, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(1, 5, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(1, 6, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(1, 7, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(1, 8, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(1, 9, "p[0] * in + p[1]", 1);

	neuralDraft.Connect(2, 4, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(2, 5, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(2, 6, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(2, 7, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(2, 8, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(2, 9, "p[0] * in + p[1]", 2);

	neuralDraft.Connect(3, 4, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(3, 5, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(3, 6, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(3, 7, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(3, 8, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(3, 9, "p[0] * in + p[1]", 3);

	neuralDraft.Connect(4, 10, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(4, 11, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(4, 12, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(4, 13, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(4, 14, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(4, 15, "p[0] * in + p[1]", 0);

	neuralDraft.Connect(5, 10, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(5, 11, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(5, 12, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(5, 13, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(5, 14, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(5, 15, "p[0] * in + p[1]", 1);

	neuralDraft.Connect(6, 10, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(6, 11, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(6, 12, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(6, 13, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(6, 14, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(6, 15, "p[0] * in + p[1]", 2);

	neuralDraft.Connect(7, 10, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(7, 11, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(7, 12, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(7, 13, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(7, 14, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(7, 15, "p[0] * in + p[1]", 3);

	neuralDraft.Connect(8, 10, "p[0] * in + p[1]", 4);
	neuralDraft.Connect(8, 11, "p[0] * in + p[1]", 4);
	neuralDraft.Connect(8, 12, "p[0] * in + p[1]", 4);
	neuralDraft.Connect(8, 13, "p[0] * in + p[1]", 4);
	neuralDraft.Connect(8, 14, "p[0] * in + p[1]", 4);
	neuralDraft.Connect(8, 15, "p[0] * in + p[1]", 4);

	neuralDraft.Connect(9, 10, "p[0] * in + p[1]", 5);
	neuralDraft.Connect(9, 11, "p[0] * in + p[1]", 5);
	neuralDraft.Connect(9, 12, "p[0] * in + p[1]", 5);
	neuralDraft.Connect(9, 13, "p[0] * in + p[1]", 5);
	neuralDraft.Connect(9, 14, "p[0] * in + p[1]", 5);
	neuralDraft.Connect(9, 15, "p[0] * in + p[1]", 5);

	neuralDraft.Connect(10, 16, "p[0] * in + p[1]", 0);
	neuralDraft.Connect(10, 17, "p[0] * in + p[1]", 0);

	neuralDraft.Connect(11, 16, "p[0] * in + p[1]", 1);
	neuralDraft.Connect(11, 17, "p[0] * in + p[1]", 1);

	neuralDraft.Connect(12, 16, "p[0] * in + p[1]", 2);
	neuralDraft.Connect(12, 17, "p[0] * in + p[1]", 2);

	neuralDraft.Connect(13, 16, "p[0] * in + p[1]", 3);
	neuralDraft.Connect(13, 17, "p[0] * in + p[1]", 3);

	neuralDraft.Connect(14, 16, "p[0] * in + p[1]", 4);
	neuralDraft.Connect(14, 17, "p[0] * in + p[1]", 4);

	neuralDraft.Connect(15, 16, "p[0] * in + p[1]", 5);
	neuralDraft.Connect(15, 17, "p[0] * in + p[1]", 5);

	/*
	
	[a11] =  [ w02 w12 ] [a00] + [b2]
	[a12]    [ w03 w13 ] [a01]   [b3]

	*/

	// because they're all identical operations
	// they can be converted into a single matrix multiplication

	auto ctxRef = neuralDraft.Construct(Aqua::Exec::Wavefront({ 16, 17 }));
	// so easy
}
