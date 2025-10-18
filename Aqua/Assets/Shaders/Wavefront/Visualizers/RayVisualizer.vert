#version 440

layout(location = 0) in vec3 aPoint;

layout(set = 0, binding = 0) uniform Camera
{
	mat4 Projection;
	mat4 ViewMatrix;
} uCamera;

void main()
{
	gl_Position = uCamera.ViewMatrix * vec4(aPoint, 1.0);
}
