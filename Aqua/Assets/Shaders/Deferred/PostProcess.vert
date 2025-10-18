#version 440 core
layout(location = 0) out vec2 vTexCoords;

vec2 positions[6] = vec2[](
	
	vec2(-1.0, -1.0),
	vec2( 1.0, -1.0),
	vec2( 1.0,  1.0),
	vec2( 1.0,  1.0),
	vec2(-1.0,  1.0),
	vec2(-1.0, -1.0)

);

vec2 texCoords[6] = vec2[](

	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(1.0, 1.0),
	vec2(1.0, 1.0),
	vec2(0.0, 1.0),
	vec2(0.0, 0.0)

);

void main()
{
	gl_Position.xy = positions[gl_VertexIndex];
	gl_Position.z = 0.0;
	gl_Position.w = 1.0;

	vTexCoords = texCoords[gl_VertexIndex];
}
