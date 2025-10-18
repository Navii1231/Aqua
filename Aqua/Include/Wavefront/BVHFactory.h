#pragma once
#include "RaytracingStructures.h"
#include "Core.h"

AQUA_BEGIN
PH_BEGIN

using SplitFunction = std::function<float(const BVH&, const Node&, int)>;

// TODO: We could add multiple functions here, each of which activate
// when a certain condition is met
struct SplitStrategy
{
	SplitFunction mSplit;
};

// TODO: The only thing remaining now is to utilize GPU to construct BVH structure

// NOTE: not thread safe
// TODO: yet to test out...
class BVHFactory
{
public:
	BVHFactory() = default;

	void SetDepth(int depth) { mDepth = depth; }
	void SetSplitStrategy(const SplitStrategy& strategy) { mStrategy = strategy; }

	template <typename VertIt, typename IdxIt>
	BVH Build(VertIt vBeg, VertIt vEnd, IdxIt iBeg, IdxIt iEnd);

	AQUA_API void Cleanup();

private:
	BVH mCurrent;
	std::vector<glm::vec3> mVertices;
	std::vector<Face> mFaces;

	int mDepth = 18;
	float mTolerence = 0.001f;

	SplitStrategy mStrategy{ DefaultSplitFn::sSpatialSplit };

public:
	struct DefaultSplitFn
	{
		static SplitFunction sObjectSplit;
		static SplitFunction sSpatialSplit;
		static SplitFunction sSAH;
	};

private:
	void Clear();

	template <typename Iter>
	void SetVertices(Iter begin, Iter end);

	template <typename Iter>
	void SetFaces(Iter begin, Iter end);


	void SplitRecursive(Node& parentNode, int depth);
	void EncloseIntoBoundingBox(Node& node);

	// vec3 and axis idx
	std::pair<float, int> GetOptimalSplit(const Node& box);

	glm::vec3 TriangleCentroid(uint32_t i);
	std::pair<Box, Box> SplitBox(const Node& node);
	std::pair<Node, Node> MakeChildNodes(const Node& parentNode, const Box& leftBox, const Box& rightBox);
};

template <typename VertIt, typename IdxIt>
BVH AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::Build(VertIt vBeg, VertIt vEnd, IdxIt iBeg, IdxIt iEnd)
{
	_STL_ASSERT(mDepth >= 0, "The depth of the BVH structure can't be negative!");

	SetVertices(vBeg, vEnd);
	SetFaces(iBeg, iEnd);

	Clear();

	Node& rootNode = mCurrent.Nodes.emplace_back();

	rootNode.BeginIndex = 0;
	rootNode.EndIndex = static_cast<uint32_t>(mFaces.size());

	EncloseIntoBoundingBox(rootNode);
	SplitRecursive(rootNode, mDepth);

	mCurrent.Vertices = std::move(mVertices);
	mCurrent.Faces = std::move(mFaces);

	return mCurrent;
}

template <typename Iter>
void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::SetFaces(Iter begin, Iter end)
{
	mFaces.assign(begin, end);
}

template <typename Iter>
void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::SetVertices(Iter begin, Iter end)
{
	mVertices.assign(begin, end);
}

PH_END
AQUA_END
