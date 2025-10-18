#version 440 core

layout(location = 0) in vec4 iPosition;
layout(location = 1) in vec4 iNormal; // [vec4.xyz --> normal] [vec4.w --> index offset]
layout(location = 2) in vec4 iTangent;
layout(location = 3) in vec4 iBitangent;
layout(location = 4) in vec3 iTexCoords;
layout(location = 5) in vec3 iMetaData;

layout(set = 0, binding = 0) uniform CameraInfo
{
	mat4 uProjection;
	mat4 uView;
};

layout(std430, set = 0, binding = 1) buffer ModelMatrices
{
	mat4 sModels[];
};

// Sending data to fragment shader
layout (location = 0) out vec4 oPosition;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oTangent;
layout (location = 3) out vec4 oBitangent;
layout (location = 4) out vec3 oTexCoords;
layout (location = 5) out vec3 oMetaData;

void main()
{
	uint ModelIdx = uint(iMetaData.r + 0.5);

	mat4 Model = sModels[ModelIdx];
	mat4 Rotation = mat4(mat3(Model));

	vec4 VertexPos = Model * iPosition;

	gl_Position = uProjection * uView * VertexPos;

	oPosition = VertexPos;
	oNormal = Rotation * iNormal;
	oTangent = Rotation * iTangent;
	oBitangent = Rotation * iBitangent;
	oTexCoords = iTexCoords;
	oMetaData = iMetaData;
}
