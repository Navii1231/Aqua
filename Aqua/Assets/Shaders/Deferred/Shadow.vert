#version 440 core

layout(location = 0) in vec4 iPosition;
layout(location = 1) in vec4 iMatMeshID;

struct CameraInfo
{
	mat4 Projection;
	mat4 View;
};

layout(push_constant) uniform ShaderConstants
{
	uint pLightSrcIdx;
};

layout(set = 0, binding = 0) readonly buffer CameraInfoBuffer
{
	CameraInfo sCameraInfos[];
};

layout(std430, set = 0, binding = 1) buffer ModelMatrices
{
	mat4 sModels[];
};

void main()
{
	uint MeshID = uint(iMatMeshID.r + 0.5);

	mat4 Model = sModels[MeshID];

	mat4 InvertY = mat4(1.0);
	InvertY[1][1] = -1.0;

	vec4 VertexPos = Model * iPosition;
	gl_Position = sCameraInfos[pLightSrcIdx].Projection * sCameraInfos[pLightSrcIdx].View * VertexPos;
}
