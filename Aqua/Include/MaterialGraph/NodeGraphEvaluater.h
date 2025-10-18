#pragma once
#include "../Core/AqCore.h"
#include "NodeGraph.h"

AQUA_BEGIN

struct Symbol
{
	std::string Type;
	std::string Name;
};

// Name --> symbol
using SymbolTable = std::unordered_map<std::string, Symbol>;

// Type --> count
using SymbolCount = std::unordered_map<std::string, int>;

struct StackFrame
{
	std::vector<SymbolTable> Symbols;
	SymbolCount SymCount;
};

class NodeGraphEvaluater
{
public:
	void SetNodeGraph(const std::shared_ptr<NodeGraph>& graph);
	std::string Evaluate() const;

private:
	std::shared_ptr<NodeGraph> mGraph;
	std::vector<StackFrame> mScopes;
};

AQUA_END

