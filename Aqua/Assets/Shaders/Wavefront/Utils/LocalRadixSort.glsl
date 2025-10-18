#version 440

layout(local_size_x = WORKGROUP_SIZE) in;

layout(set = 0, binding = 0) buffer InputBuffer
{
	uint sElements[];
};

layout(push_constant) uniform PrefixData
{
	uint pBufferSize;
	uint pBitCount;
};

shared uint sPrefixSum[WORKGROUP_SIZE * STRIDE];
shared uint sLocalElements[WORKGROUP_SIZE * STRIDE];

uint PowerOfTwo(uint Val)
{
	return 1 << Val;
}

void LocalPrefixSum(in uint LocalIdx, in uint GlobalIdx)
{
#define ARRAY sPrefixSum
#define LOCAL_STRIDE STRIDE

	//uint TreeDepth = uint(log2(WORKGROUP_SIZE) + 0.5);

	// Up sweep phase...

	for (uint i = 0; i < TREE_DEPTH; i++)
	{
		if (LocalIdx + PowerOfTwo(i + 1) <= WORKGROUP_SIZE && LocalIdx % PowerOfTwo(i + 1) == 0)
		{
			uint Temp = ARRAY[(GlobalIdx + PowerOfTwo(i) - 1) * LOCAL_STRIDE];
			ARRAY[(GlobalIdx + PowerOfTwo(i + 1) - 1) * LOCAL_STRIDE] += Temp;
		}

		barrier();
	}

	// Down sweep phase...

	if (LocalIdx == WORKGROUP_SIZE - 1)
		ARRAY[(GlobalIdx) * LOCAL_STRIDE] = 0;

	for (int i = int(TREE_DEPTH - 1); i >= 0; i--)
	{
		barrier();

		if (LocalIdx + PowerOfTwo(i + 1) <= WORKGROUP_SIZE && LocalIdx % PowerOfTwo(i + 1) == 0)
		{
			uint Temp = ARRAY[(GlobalIdx + PowerOfTwo(i) - 1) * LOCAL_STRIDE];

			ARRAY[(GlobalIdx + PowerOfTwo(i) - 1) * LOCAL_STRIDE] =
				ARRAY[(GlobalIdx + PowerOfTwo(i + 1) - 1) * LOCAL_STRIDE];

			ARRAY[(GlobalIdx + PowerOfTwo(i + 1) - 1) * LOCAL_STRIDE] += Temp;
		}
	}
}

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;
	uint LocalIdx = gl_LocalInvocationID.x;

	if (GlobalIdx >= pBufferSize)
		return;

	sLocalElements[LocalIdx * STRIDE] = sElements[GlobalIdx];

	//const bool Descending = gl_WorkGroupID.x % 2 == 1;
	const bool Descending = false;

	barrier();

	for (uint BitIndex = 0; BitIndex < pBitCount; BitIndex++)
	{
		// Collect the falses
		uint IsTrue = (sLocalElements[LocalIdx * STRIDE] >> BitIndex) & 1;

		sPrefixSum[LocalIdx * STRIDE] = Descending ? IsTrue : 1 - IsTrue;

		// Last bit value will used to calculate the total number of falses
		uint LastBitVal = 1 - (sLocalElements[(WORKGROUP_SIZE - 1) * STRIDE] >> BitIndex) & 1;

		LastBitVal = Descending ? 1 - LastBitVal : LastBitVal;

		uint ElemVal = sLocalElements[LocalIdx * STRIDE];

		barrier();

		// Scan the false values
		LocalPrefixSum(LocalIdx, LocalIdx);

		barrier();

		// Total number of false keys
		uint TotalFalses = LastBitVal + sPrefixSum[(WORKGROUP_SIZE - 1) * STRIDE];

		// Calculate the destination index and scatter the elements to their destination
		uint Index = IsTrue == uint(!Descending) ?
			LocalIdx - sPrefixSum[LocalIdx * STRIDE] + TotalFalses :
			sPrefixSum[LocalIdx * STRIDE];

		//uint Index = uint(Descending) * sPrefixSum[LocalIdx * STRIDE] +
		//	uint(!Descending) * (LocalIndex - sPrefixSum[LocalIdx * STRIDE] + TotalFalses);

		sLocalElements[Index * STRIDE] = ElemVal;

		barrier();
	}

	sElements[GlobalIdx] = sLocalElements[LocalIdx * STRIDE];
}
