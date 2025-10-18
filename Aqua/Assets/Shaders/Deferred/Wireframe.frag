#version 440 core
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec3 vMetaData;

layout(push_constant) uniform ShaderConstants
{
	vec4 pColor;
};

void main()
{
	FragColor = pColor;
	//FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
