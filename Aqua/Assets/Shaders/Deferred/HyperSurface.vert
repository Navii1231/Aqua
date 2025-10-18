#version 440 core

layout(location = 0) in vec4 aPoint;
layout(location = 1) in vec4 aColor;

layout(push_constant) uniform ShaderConstants
{
	float pPointSize;
};

layout(std140, set = 0, binding = 0) uniform Camera
{
	mat4 uProjection;
	mat4 uView;
};

layout(location = 0) out vec4 vColor;

void main()
{
	gl_PointSize = pPointSize;

	gl_Position = uProjection * uView * aPoint;
	vColor = aColor;
}
