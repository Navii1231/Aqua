#pragma once
#include "../Core/AqCore.h"
#include "MaterialNode.h"

AQUA_BEGIN

// The root node is material output

class NodeGraph
{
public:
	NodeGraph() = default;
	~NodeGraph() = default;

	MaterialNode* Evaluate() const;

private:
	MaterialNode* mRoot;
};

AQUA_END
