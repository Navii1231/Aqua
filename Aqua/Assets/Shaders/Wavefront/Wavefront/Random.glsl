#ifndef RANDOM_GLSL
#define RANDOM_GLSL

#define MATH_PI 3.14159265358979323846

float GetRandom(inout uint state)
{
	state *= state * 747796405 + 2891336453;
	uint result = ((state >> (state >> 28) + 4) ^ state) * 277803737;
	result = (result >> 22) ^ result;
	return result / 4294967295.0;
}

vec2 SampleOnUnitDisk(inout uint state)
{
	float Radius = GetRandom(state);
	float Theta = 2.0 * MATH_PI * GetRandom(state);

	vec2 UV = vec2(cos(Theta), sin(Theta));

	return Radius * UV;
}

#endif