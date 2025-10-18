#include "Core/Aqpch.h"
#include "Wavefront/BVHFactory.h"
#include "Utils/BisectionSolver.h"

AQUA_BEGIN
PH_BEGIN

glm::vec3 TriangleCentroid(const BVH& bvh, uint32_t i)
{
	glm::vec3 A = bvh.Vertices[bvh.Faces[i].Indices.x];
	glm::vec3 B = bvh.Vertices[bvh.Faces[i].Indices.y];
	glm::vec3 C = bvh.Vertices[bvh.Faces[i].Indices.z];

	return (A + B + C) / 3.0f;
}

// TODO: Very expensive, there should be a way to optimize it
uint32_t CountTriangles(const BVH& bvh, const Node& node, float lambda, float split, int index)
{
	if (lambda >= 1.0f)
		return node.EndIndex - node.BeginIndex;

	if (lambda <= 0.0f)
		return 0;

	glm::vec3 offset = glm::vec3(0.0f);
	offset[index] = lambda * split;

	Box box(node.MinBound, node.MinBound + offset);

	uint32_t count = 0;

	for (uint32_t begin = node.BeginIndex; begin < node.EndIndex; begin++)
	{
		if (box.IsInside(TriangleCentroid(bvh, begin)))
			count++;
	}

	return count;
}

float SpatialSplit(const BVH& bvh, const Node& node, int index)
{
	return (node.MaxBound[index] - node.MinBound[index]) / 2.0f;
}

float ObjectSplit(const BVH& bvh, const Node& node, int index)
{
	// Using bisection method to find the root of this equation
	// f(lambda) = N(lambda) - No / 2

	fBisectionSolver solver{};

	float splitDistance = node.MaxBound[index] - node.MinBound[index];
	float TriCount = static_cast<float>(node.EndIndex - node.BeginIndex);

	solver.SetFunc([&bvh, &node, splitDistance, TriCount, index](float lambda)->float
	{
		return (float) CountTriangles(bvh, node, lambda, splitDistance, index) - (float) TriCount / 2.0f;
	});

	solver.SetTolerence(1.5f);
	float root = solver.Solve(0.0f, 1.0f);

	return root;
}

PH_END
AQUA_END

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::SplitFunction 
	AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::DefaultSplitFn::sSpatialSplit = SpatialSplit;

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::SplitFunction
AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::DefaultSplitFn::sObjectSplit = ObjectSplit;

// TODO: We have yet to implement the Surface Area Heuristic (SAH) split

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::Cleanup()
{
	Clear();

	mCurrent.Vertices.clear();
	mCurrent.Faces.clear();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::Clear()
{
	mCurrent.Nodes.clear();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::SplitRecursive(Node& parentNode, int depth)
{
	if (parentNode.EndIndex - parentNode.BeginIndex < 2)
		return;

	if (depth == 0)
		return;

	auto [leftBox, rightBox] = SplitBox(parentNode);
	auto [leftChild, rightChild] = MakeChildNodes(parentNode, leftBox, rightBox);

	if (leftChild.BeginIndex == leftChild.EndIndex || rightChild.BeginIndex == rightChild.EndIndex)
		return;

	parentNode.FirstChildIndex = (uint32_t) mCurrent.Nodes.size();
	parentNode.SecondChildIndex = parentNode.FirstChildIndex + 1;

	uint32_t leftBoxIndex = parentNode.FirstChildIndex;
	uint32_t secondBoxIndex = parentNode.SecondChildIndex;

	mCurrent.Nodes.emplace_back(leftChild);
	mCurrent.Nodes.emplace_back(rightChild);

	// Split the left and right box recursively
	SplitRecursive(mCurrent.Nodes[leftBoxIndex], depth - 1);
	SplitRecursive(mCurrent.Nodes[secondBoxIndex], depth - 1);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::EncloseIntoBoundingBox(Node& node)
{
	glm::vec3 MinBound = { FLT_MAX, FLT_MAX, FLT_MAX };
	glm::vec3 MaxBound = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	auto ReplaceMinBound = [](float& MinBound, float VertexComponent)
	{
		if (MinBound > VertexComponent)
			MinBound = VertexComponent;
	};

	auto ReplaceMaxBound = [](float& MaxBound, float VertexComponent)
	{
		if (MaxBound < VertexComponent)
			MaxBound = VertexComponent;
	};

	std::for_each(mFaces.begin() + node.BeginIndex, mFaces.begin() + node.EndIndex,
		[this, &MinBound, &MaxBound, ReplaceMaxBound, ReplaceMinBound](const Face& face)
	{
		for (int i = 0; i < 3; i++)
		{
			glm::vec3 Vertex = mVertices[face.Indices[i]];

			ReplaceMinBound(MinBound.x, Vertex.x);
			ReplaceMinBound(MinBound.y, Vertex.y);
			ReplaceMinBound(MinBound.z, Vertex.z);

			ReplaceMaxBound(MaxBound.x, Vertex.x);
			ReplaceMaxBound(MaxBound.y, Vertex.y);
			ReplaceMaxBound(MaxBound.z, Vertex.z);
		}
	});

	node.MinBound = MinBound - glm::vec3(mTolerence);
	node.MaxBound = MaxBound + glm::vec3(mTolerence);
}

std::pair<float, int> AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::GetOptimalSplit(const Node& node)
{
	// First find the longest axis

	glm::vec3 BoxSpan = node.MaxBound - node.MinBound;

	float MaxSpan = 0.0f;
	int LargestSpanIndex = 0;

	for (int i = 0; i < 3; i++)
	{
		if (MaxSpan < BoxSpan[i])
		{
			LargestSpanIndex = i;
			MaxSpan = BoxSpan[i];
		}
	}

	// We will split along the longest axis
	float splitPos = mStrategy.mSplit(mCurrent, node, LargestSpanIndex);

	return { splitPos, LargestSpanIndex };
}

glm::vec3 AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::TriangleCentroid(uint32_t i)
{
	glm::vec3 A = mVertices[mFaces[i].Indices.x];
	glm::vec3 B = mVertices[mFaces[i].Indices.y];
	glm::vec3 C = mVertices[mFaces[i].Indices.z];

	return (A + B + C) / 3.0f;
}

std::pair<AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Box, AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Box> 
	AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::SplitBox(const Node& node)
{
#if 0
	glm::vec3 BoxSpan = node.MaxBound - node.MinBound;

	float MaxSpan = 0.0f;
	int LargetSpanIndex = 0;

	for (int i = 0; i < 3; i++)
	{
		if (MaxSpan < BoxSpan[i])
		{
			LargetSpanIndex = i;
			MaxSpan = BoxSpan[i];
		}
	}

	float HalfMaxSpan = MaxSpan / 2.0f;
#endif

	auto [Span, LargestSpanIndex] = GetOptimalSplit(node);

	const glm::vec3 Offset = glm::vec3(mTolerence);

	glm::vec3 MaxBoundForLeftBox = node.MaxBound + Offset;
	MaxBoundForLeftBox[LargestSpanIndex] -= Span;

	glm::vec3 MinBoundForRightBox = node.MinBound - Offset;
	MinBoundForRightBox[LargestSpanIndex] += Span;

	return { Box(node.MinBound - Offset, MaxBoundForLeftBox),
		Box(MinBoundForRightBox, node.MaxBound + Offset) };
}

std::pair<AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Node, AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Node> 
	AQUA_NAMESPACE::PH_FLUX_NAMESPACE::BVHFactory::MakeChildNodes(
		const Node& parentNode, const Box& leftBox, const Box& rightBox)
{
	// Partition the points for spatial coherence
	uint32_t LeftIndex = parentNode.BeginIndex;

	for (uint32_t i = parentNode.BeginIndex; i < parentNode.EndIndex; i++)
	{
		glm::vec3 Centre = TriangleCentroid(i);

		if (leftBox.IsInside(Centre))
		{
			if (i != LeftIndex)
				std::swap(mFaces[i], mFaces[LeftIndex]);

			LeftIndex++;
		}
		else
		{
			_STL_ASSERT(rightBox.IsInside(Centre), "Point found neither in the left or right bounding box!");
		}
	}

	// Construct the child bounding boxes
	Node leftChild, rightChild;
	leftChild.BeginIndex = parentNode.BeginIndex;
	leftChild.EndIndex = LeftIndex;
	leftChild.FirstChildIndex = 0;
	leftChild.SecondChildIndex = 0;
	leftChild.MinBound = leftBox.Min;
	leftChild.MaxBound = leftBox.Max;

	rightChild.BeginIndex = LeftIndex;
	rightChild.EndIndex = parentNode.EndIndex;
	rightChild.FirstChildIndex = 0;
	rightChild.SecondChildIndex = 0;
	rightChild.MinBound = rightBox.Min;
	rightChild.MaxBound = rightBox.Max;

	EncloseIntoBoundingBox(leftChild);
	EncloseIntoBoundingBox(rightChild);

	return { leftChild, rightChild };
}
