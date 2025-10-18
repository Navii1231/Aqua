#version 440 core

layout (set = 0, binding = 0) uniform CameraInfo
{
	mat4 uProjection;
	mat4 uView;
};

layout (location = 0) out vec3 vCameraCorners;

vec2 positions[6] = vec2[](

	vec2(-1.0, -1.0),
	vec2( 1.0, -1.0),
	vec2( 1.0,  1.0),
	vec2( 1.0,  1.0),
	vec2(-1.0,  1.0),
	vec2(-1.0, -1.0)

);

vec3 cameraCorners[6] = vec3[](
	
	normalize(vec3(-1.0,  1.0, 1.0)),
	normalize(vec3( 1.0,  1.0, 1.0)),
	normalize(vec3( 1.0, -1.0, 1.0)),
	normalize(vec3( 1.0, -1.0, 1.0)),
	normalize(vec3(-1.0, -1.0, 1.0)),
	normalize(vec3(-1.0,  1.0, 1.0))

);

void main()
{
	gl_Position.xy = positions[gl_VertexIndex];
	gl_Position.z = 1.0;
	gl_Position.w = 1.0;

	vCameraCorners = normalize(transpose(mat3(uView)) * cameraCorners[gl_VertexIndex]);
}
