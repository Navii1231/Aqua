#version 440

layout(local_size_x = WORKGROUP_SIZE) in;

layout(std430, set = 0, binding = 0) buffer Elements
{
	uint sElements[];
};

layout(push_constant) uniform MetaData
{
	uint pBufferSize;
};

void main()
{
	uint GlobalIdx = gl_GlobalInvocationID.x;

	if (GlobalIdx != 0 && pBufferSize > 0)
		return;

	// TODO: Using sequencial algorithm for now as we only have small arrays (size of 16 or less)
	for (uint i = 0; i < pBufferSize - 1; i++)
	{
		sElements[i + 1] += sElements[i];
	}

	for (uint i = pBufferSize - 1; i > 0; i--)
	{
		sElements[i] = sElements[i - 1];
	}

	sElements[0] = 0;
}
