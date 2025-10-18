
// version goes here but will be inserted by the user
// So we put a macro as a placeholder for the version
#version 440 core

layout(local_size_x = WORK_GROUP_SIZE_X, local_size_y = WORK_GROUP_SIZE_Y) in;

struct Camera
{
    mat4 Projection;
    mat4 View;
};

struct DirectionalLightSrc
{
    vec3 Color;
    vec3 Direction;
};

struct PointLightSrc
{
    vec3 Position;
    vec3 Intensity;
    vec2 DropRate;
};

struct BSDFInput
{
    vec3 ViewDir;
    vec3 LightDir;
    vec3 Normal;
};

layout(push_constant) uniform ShaderConstants
{
	// Material and ray stuff...
    uvec2 pSize;
	uint pDirectionalLightCount;
	uint pPointLightCount;
    uint pMaterialRef;
};

/***************** These set bindings could be stored in a separate file ********************/

layout(set = 0, binding = 1) uniform sampler2D uPositions;
layout(set = 0, binding = 2) uniform sampler2D uNormals;
layout(set = 0, binding = 3) uniform sampler2D uTexCoords;
layout(set = 0, binding = 4) uniform sampler2D uTangents;
layout(set = 0, binding = 5) uniform sampler2D uBitangents;

layout(set = 0, binding = 6) uniform sampler2D uDepthMap[MAX_DEPTH_ARRAY];

layout(std430, set = 1, binding = 0) readonly buffer DirectionalLights
{
    DirectionalLightSrc sDirectionalLights[];
};

layout(std430, set = 1, binding = 1) readonly buffer PointLights
{
    PointLightSrc sPointLights[];
};

layout(std430, set = 1, binding = 2) readonly buffer DepthCameras
{
    Camera uDepthCameras[];
};

layout(std430, set = 1, binding = 3) readonly buffer Models
{
    mat4 sModels[];
};

layout(std140, set = 1, binding = 4) uniform CameraUniform
{
    Camera uCamera;
};

/****************************************************************/

/* Declaration of shader parameters
* Example:

struct __ShaderParameter
{
    vec3 base_color;
    float roughness;
};

layout(std430, set = 1, binding = 5) readonly buffer __ShaderParameters
{
    __ShaderParameter __sShaderParameters[];
};

* The above piece of code is just one example of how shader parameters could manifest themselves
* for declarations like @vec3.base_color and @float.roughness

*/