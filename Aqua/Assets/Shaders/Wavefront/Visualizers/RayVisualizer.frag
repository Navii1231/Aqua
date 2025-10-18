#version 440

layout(location = 0) out vec3 FragColor;

layout(push_constant) uniform Color
{
	vec3 pColor;
};

void main()
{
	FragColor = pColor;
}
