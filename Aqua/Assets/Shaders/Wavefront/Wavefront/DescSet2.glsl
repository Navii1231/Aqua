#ifndef DESC_SET_2_GLSL
#define DESC_SET_2_GLSL

layout(std430, set = 2, binding = 0) buffer CPU_Feedback
{
	uint Finished;
} uCPU_Feedback;

#endif