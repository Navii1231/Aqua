#pragma once
#include "Application.h"
#include "Layer.h"
#include "Execution/Draft.h"

struct Dependency
{
	Aqua::Exec::NodeID From, To;
	Aqua::SharedRef<std::binary_semaphore> Semaphore;
};

struct ExecNode
{
	std::function<bool()> mFn;
	std::vector<Dependency> mInputs, mOutputs;
};

struct DependencyInfo
{

};

using ExecNodeRef = Aqua::SharedRef<ExecNode>;

class CPUDraft : public Aqua::Exec::Draft<ExecNode, DependencyInfo>
{
public:
	CPUDraft() = default;

	// the graph could be submitted into a thread pool and viola!
	// dependencies and data transfer should take care of themselves
	std::expected<Aqua::Exec::BasicGraph<ExecNodeRef>, Aqua::Exec::GraphError> Construct(const Aqua::Exec::Wavefront& probes)
	{
		return _ConstructEx<ExecNodeRef>(probes, true, [this](Aqua::Exec::NodeID Id, const MyNodeInfo& info)->std::expected<ExecNodeRef, Aqua::Exec::GraphError>
			{
				return Aqua::MakeRef(info);
			}, [this](const NodeInfo<ExecNodeRef>& from, const NodeInfo<ExecNodeRef>& to, const DependencyInfo& depInfo)->std::expected<bool, Aqua::Exec::GraphError>
				{
					auto& dependency = to.ExprNode->mInputs.emplace_back();

					dependency.From = from.ID;
					dependency.To = to.ID;

					dependency.Semaphore = Aqua::MakeRef<std::binary_semaphore>(0);

					from.ExprNode->mOutputs.push_back(dependency);

					return true;
				});
	}

private:

};
