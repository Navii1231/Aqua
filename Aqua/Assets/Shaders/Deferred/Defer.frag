#version 440 core

layout(location = 0) out vec4 oPosition;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oTexCoords;
layout(location = 3) out vec4 oTangent;
layout(location = 4) out vec4 oBitangent;

// Receving data from the vertex shader
layout (location = 0) in vec4 iPosition;
layout (location = 1) in vec4 iNormal;
layout (location = 2) in vec4 iTangent;
layout (location = 3) in vec4 iBitangent;
layout (location = 4) in vec3 iTexCoords;
layout (location = 5) in vec3 iMetaData;

void main()
{
	// The TextureCoords and MeshMatID all could be stored inside the alpha channels
	oPosition = iPosition;
	oNormal = iNormal;
	oTangent = iTangent;
	oBitangent = iBitangent;
	oTexCoords.xyz = iTexCoords;

	oPosition.w = iMetaData.r;
	oNormal.w = iMetaData.g;
	oTexCoords.w = iMetaData.b;
}