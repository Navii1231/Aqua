#pragma once
#include "../Core/AqCore.h"
#include "../Core/SharedRef.h"

AQUA_BEGIN
EXEC_BEGIN

// Aqua Flow execution model

struct GenericNode;

using NodeID = uint64_t;

enum class GraphTraversalState
{
	ePending                            = 0,
	eVisiting                           = 1,
	eVisited                            = 2,
};

// todo: we might even allow multiple tasks at once in a single operation
enum class OpType
{
	eNone                               = 0,
	eCPU                                = 1,
	eCompute                            = 2,
	eGraphics                           = 4,
	eRayTracing                         = 8,
	eCopyOrTransfer                     = 16,
	eTransition                         = 32,
};

enum class State
{
	ePending                            = 0,
	eReady                              = 1,
	eExecute                            = 2,
};

enum class GraphError
{
	ePathDoesntExist                    = 1,
	ePathReferencedMoreThanOnce         = 2,
	eInvalidConnection                  = 3,
	eDependencyUponItself               = 4,
	eFoundEmbeddedCircuit               = 5,
	eInjectedOpDoesntExist              = 6,
	eFailedToConstructNode               = 7,
};

struct DependencyMetaData
{
	NodeID From;
	NodeID To;
	vk::PipelineStageFlags WaitingStage;
};

struct InjectionMetaData
{
	vk::PipelineStageFlags WaitPoint;
	vkLib::Core::Ref<vk::Semaphore> Signal;
};

struct DescriptorHasher
{
	size_t operator()(const ::VK_NAMESPACE::DescriptorLocation& location) const
	{
		return ::VK_NAMESPACE::CreateHash(location);
	}
};

struct GraphRsc
{
	// type of the buffer or image
	std::string Typename = "fp32";

	// location and name - unique across whole graph
	vkLib::DescriptorLocation Location;
	std::string Name;

	// buffer rsc
	vkLib::GenericBuffer Buffer;

	// image rsc
	vk::Format Format = vk::Format::eR8G8B8A8Unorm;
	vkLib::ImageView ImageView;
	vkLib::Core::Ref<vk::Sampler> Sampler;

	// operations sharing this rsc
	std::set<NodeID> Operations;

	// descriptor type
	vk::DescriptorType Type = vk::DescriptorType::eStorageBuffer;

	bool operator==(const GraphRsc& other) const
	{
		return Typename == other.Typename && Location == other.Location && Name == other.Name && Type == other.Type;
	}

	bool operator!=(const GraphRsc& other) const { return !operator==(other); }
};

using GraphRscMap = std::unordered_map<vkLib::DescriptorLocation, GraphRsc, DescriptorHasher>;

EXEC_END
AQUA_END
