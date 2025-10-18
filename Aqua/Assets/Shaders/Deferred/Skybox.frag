#version 440 core

layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 1) uniform sampler2D uEnvironmentMap;

layout (location = 0) in vec3 vCameraCorners;

vec3 SampleCubeMap(in vec3 dir, float rotation)
{
	float Theta = acos(dir.y);
	float phi = atan(dir.x / dir.z);

	phi += rotation + MATH_PI / 2.0;

	if(dir.z < 0.0)
		phi += MATH_PI;

	vec2 uv = vec2(phi / (2.0 * MATH_PI), Theta / MATH_PI);

	return texture(uEnvironmentMap, uv).rgb;
}

void main()
{
	FragColor.rgb = SampleCubeMap(normalize(vCameraCorners), 0.0);
	FragColor.a = 1.0;

	//FragColor = vec4(1.0);
}
