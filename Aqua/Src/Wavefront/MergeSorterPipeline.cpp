#include "Core/Aqpch.h"
#include "Wavefront/MergeSorterPipeline.h"

// This code should also be found in $(ProjectDirectory)/Shaders/Utils/ directory...

std::string gMergeSortCode = 
R"(

#version 440

layout(local_size_x = WORKGROUP_SIZE) in;

// Front end of the merge sort shader

// The ArrayRef struct is used to avoid high memory bandwidth
// The type 'PRIMITIVE_TYPE' has be defined by the user
// It has to be a 4-byte builtin type like int, uint, float etc...
// For example: #define PRIMITIVE_TYPE uint
struct ArrayRef
{
	PRIMITIVE_TYPE CompareElem;
	uint ElemIdx;
};

layout(push_constant) uniform MetaData
{
	uint pBufferSize;
	uint pActiveBuffer;
	uint pTreeDepth;
	uint pSectionLength;
};

// Back end of the merge sort shader

layout(std430, set = 0, binding = 0) buffer InputBuffer
{
	ArrayRef sBuffer[];
};

void Equalize(uint DstIdx, uint SrcIdx)
{
	const uint InactiveBuffer = 1 - pActiveBuffer;

	sBuffer[InactiveBuffer * pBufferSize + DstIdx] =
		sBuffer[pActiveBuffer * pBufferSize + SrcIdx];
}

uint LeftMostBinarySearch(PRIMITIVE_TYPE Target, uint Begin, uint End)
{
	if (Begin == End)
		return 0;

	uint WindowBegin = Begin;
	uint WindowEnd = End;

	for(uint depth = pTreeDepth; depth > 1; depth--)
	{
		uint MidIdx = (WindowBegin + WindowEnd) / 2;
		PRIMITIVE_TYPE Elem = sBuffer[pActiveBuffer * pBufferSize + MidIdx].CompareElem;

		bool IsLesser = Elem < Target;

		WindowBegin = IsLesser ? MidIdx : WindowBegin;
		WindowEnd = !IsLesser ? MidIdx : WindowEnd;
	}

	if (sBuffer[pActiveBuffer * pBufferSize + WindowBegin].CompareElem >= Target)
		return WindowBegin - Begin;

	return WindowEnd - Begin;
}

uint RightMostBinarySearch(PRIMITIVE_TYPE Target, uint Begin, uint End)
{
	if (Begin == End)
		return 0;

	uint WindowBegin = Begin;
	uint WindowEnd = End;

	for(uint depth = pTreeDepth; depth > 1; depth--)
	{
		uint MidIdx = (WindowBegin + WindowEnd) / 2;
		PRIMITIVE_TYPE Elem = sBuffer[pActiveBuffer * pBufferSize + MidIdx].CompareElem;

		bool IsGreater = Elem > Target;

		WindowEnd = IsGreater ? MidIdx : WindowEnd;
		WindowBegin = !IsGreater ? MidIdx : WindowBegin;
	}

	if (sBuffer[pActiveBuffer * pBufferSize + WindowBegin].CompareElem > Target)
		return WindowBegin - Begin;

	return WindowEnd - Begin;
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	uint SectionIdx = GlobalIdx / pSectionLength;
	uint SectionOffset = pSectionLength == 1 ? 0 : GlobalIdx % pSectionLength;

	const uint InactiveBuffer = 1 - pActiveBuffer;

	uint Idx = 2 * SectionIdx * pSectionLength + SectionOffset;

	uint Begin = (2 * SectionIdx) * pSectionLength;
	uint Mid = (2 * SectionIdx + 1) * pSectionLength;
	uint End = (2 * SectionIdx + 2) * pSectionLength;

	Begin = Begin < pBufferSize ? Begin : pBufferSize;
	Mid = Mid < pBufferSize ? Mid : pBufferSize;
	End = End < pBufferSize ? End : pBufferSize;

	if (Begin == End)
		return;

	// Placing right sub-array elements in their proper positions

	if (Idx < pBufferSize)
	{
		uint IdxLeft = LeftMostBinarySearch(
			sBuffer[pActiveBuffer * pBufferSize + Idx].CompareElem, Mid, End);

		uint LeftLocalIdx = IdxLeft + Idx;

		Equalize(LeftLocalIdx, Idx);
	}

	// Placing left sub-array elements in their proper positions

	if (Idx + pSectionLength < pBufferSize)
	{
		uint IdxRight = RightMostBinarySearch(
			sBuffer[pActiveBuffer * pBufferSize + Idx + pSectionLength].CompareElem, Begin, Mid);

		uint RightLocalIdx = IdxRight + Idx;

		Equalize(RightLocalIdx, Idx + pSectionLength);
	}
}

)";

std::string AQUA_NAMESPACE::PH_FLUX_NAMESPACE::GetMergeSortCode()
{
	return gMergeSortCode;
}
