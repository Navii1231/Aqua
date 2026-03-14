#pragma once
#include "IRNeuralDraft.h"

struct LinkingInfo
{
	Aqua::Exec::Wavefront InputNodes, OutputNodes;
};

struct Executable
{
	Aqua::Exec::BasicGraph<ExprNodeRef> Executable;

	// other related stuff
};

class IRLinker : public Aqua::Exec::Draft<IRContext, LinkingInfo>
{
public:
	IRLinker() = default;
	~IRLinker() = default;

	Executable Construct();

private:

};

void TestLinker()
{
	IRLinker linker{};
	LinkingInfo l12, l23;

	linker[1] = {};
	linker[2] = {};
	linker[3] = {};

	linker.Connect(1, 2, l12);
	linker.Connect(2, 3, l23);

	auto executable = linker.Construct();
}
