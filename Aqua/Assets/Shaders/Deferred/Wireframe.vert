#version 440 core

// Any material that Aqua::Renderer accepts must have the same vertex shader
// you can query the vertex shader from the renderer

layout(location = 0) in vec4 aPos;
layout(location = 1) in vec3 aMetaData;

layout(location = 0) out vec3 fMetaData;

layout(std140, set = 0, binding = 0) uniform Camera
{
	mat4 uProjection;
	mat4 uView;
};

layout(std430, set = 0, binding = 1) readonly buffer Models
{
	mat4 sModels[];
};

void main()
{
	uint MeshIdx = uint(aMetaData.r + 0.5);
	mat4 Model = sModels[MeshIdx];
	gl_Position = uProjection * uView * Model * aPos;

	fMetaData = aMetaData;
}
