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
	uint pLargestElem;
};

// Back end of the merge sort shader

layout(std430, set = 0, binding = 0) buffer RefBuffer
{
	ArrayRef sBuffer[];
};

layout(std430, set = 0, binding = 1) buffer CountBuffer
{
	uint sCounts[];
};

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx >= pBufferSize)
		return;

	for (uint i = 0; i < pLargestElem; i++)
	{
		atomicAdd(sCounts[i], uint(sBuffer[GlobalIdx].CompareElem == i));
	}
}
