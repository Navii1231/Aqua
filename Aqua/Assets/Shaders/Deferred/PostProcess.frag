#version 440 core
layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 vTexCoords;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

void main()
{
	FragColor = texture(uTexture, vTexCoords);
}
