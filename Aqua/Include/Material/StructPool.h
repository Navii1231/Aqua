#pragma once
#include "MaterialConfig.h"

AQUA_BEGIN

enum class StructPoolError
{
	eBlockSizeIsSmallerThanMaxAlignment               = 1,
};

template <typename SizeType>
class StructPool
{
public:
	struct Element
	{
		SizeType Size;
		SizeType Alignment;
		SizeType OriginalIdx;
	};

	struct Placement
	{
		SizeType BlockIndex;
		SizeType Offset;
		SizeType GlobalOffset;
		SizeType Size;
		SizeType Alignment;
		SizeType RefIdx;
	};

	using ElementList = std::vector<Element>;
	using PlacementList = std::vector<Placement>;

public:
	StructPool() = default;
	~StructPool() = default;

	void Clear();
	
	void PushElement(SizeType size, SizeType alignment)
	{ mElements.push_back({ size, alignment, static_cast<SizeType>(mElements.size())}); }

	void SetBlockSize(SizeType blockSize)
	{ mBlockSize = blockSize; }

	void SetBlockSizeAsMaxAlignment()
	{ mBlockSize = FindMaxAlignment(); }

	SizeType GetBlockSize() const { return mBlockSize; }

	// Main packing function
	std::expected<std::vector<Placement>, StructPoolError> PackElements();

private:
	SizeType mBlockSize;
	ElementList mElements;
	PlacementList mPlacements;

	// helper function
	// Recursive placement into a fragment
	bool TryPlaceElement(SizeType blockIndex, SizeType fragmentStart, SizeType fragmentSize, SizeType& currIdx);

	// Aligns a given offset to the required alignment
	SizeType AlignTo(SizeType offset, SizeType alignment)
	{ return (offset + alignment - 1) & ~(alignment - 1); }

	SizeType FindMaxAlignment();
	std::expected<bool, StructPoolError> DoSomeSanityChecksHere();
};

AQUA_END

template <typename SizeType>
bool AQUA_NAMESPACE::StructPool<SizeType>::TryPlaceElement(SizeType blockIndex, SizeType fragmentStart, SizeType fragmentSize, SizeType& currIdx)
{
	if (currIdx >= mElements.size())
		return false;

	SizeType globalOffset = blockIndex * mBlockSize + fragmentStart;
	SizeType alignedOffset = AlignTo(globalOffset, mElements[currIdx].Alignment);
	SizeType localOffset = alignedOffset - blockIndex * mBlockSize;

	if (mElements[currIdx].Size > fragmentSize)
		return false;

	// Place the element and the update the curr idx
	mPlacements.push_back({ blockIndex, localOffset, globalOffset, mElements[currIdx].Size,
		mElements[currIdx].Alignment, mElements[currIdx].OriginalIdx });
	uint32_t pushedIdx = currIdx;
	currIdx++;

	// Left fragment
	if (localOffset > fragmentStart)
		TryPlaceElement(blockIndex, fragmentStart, localOffset - fragmentStart, currIdx);

	// Right fragment
	SizeType rightStart = localOffset + mElements[pushedIdx].Size;

	if (rightStart < fragmentStart + fragmentSize)
		TryPlaceElement(blockIndex, rightStart, fragmentStart + fragmentSize - rightStart, currIdx);

	return true;
}

template <typename SizeType>
std::expected<std::vector<typename AQUA_NAMESPACE::StructPool<SizeType>::Placement>, AQUA_NAMESPACE::StructPoolError> 
	AQUA_NAMESPACE::StructPool<SizeType>::PackElements()
{
	mPlacements.clear();

	if (mElements.empty())
		return mPlacements;

	auto error = DoSomeSanityChecksHere();

	if (!error)
		return std::unexpected(error.error());

	mPlacements.reserve(mElements.size());

	// Sort elements by descending size (or alignment if preferred)
	std::sort(mElements.begin(), mElements.end(), [](const Element& a, const Element& b)
		{
			return a.Size > b.Size;
		});

	SizeType blockIndex = 0;
	SizeType elemIdx = 0;

	while (elemIdx < mElements.size())
	{
		TryPlaceElement(blockIndex, 0, mBlockSize, elemIdx);
		++blockIndex;
	}

	return mPlacements;
}

template <typename SizeType>
void AQUA_NAMESPACE::StructPool<SizeType>::Clear()
{
	mBlockSize = 0;
	mElements.clear();
	mPlacements.clear();
}

template <typename SizeType>
SizeType AQUA_NAMESPACE::StructPool<SizeType>::FindMaxAlignment()
{
	SizeType maxBlockSize = 0;

	std::for_each(mElements.begin(), mElements.end(), [&maxBlockSize](const Element& elem)
		{
			if (maxBlockSize < elem.Alignment)
				maxBlockSize = elem.Alignment;
		});

	return maxBlockSize;
}

template <typename SizeType>
std::expected<bool, AQUA_NAMESPACE::StructPoolError> AQUA_NAMESPACE::StructPool<SizeType>::DoSomeSanityChecksHere()
{
	// checking of the block size is greater than the maximum alignment in the element list

	SizeType maxBlockSize = FindMaxAlignment();

	if (maxBlockSize > mBlockSize)
		return std::unexpected(StructPoolError::eBlockSizeIsSmallerThanMaxAlignment);

	return true;
}
