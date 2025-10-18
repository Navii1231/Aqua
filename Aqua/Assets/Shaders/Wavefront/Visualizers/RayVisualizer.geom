#version 440

layout(lines) in;               // Input primitive: two points (one line segment)
layout(line_strip, max_vertices = 2) out; // Output primitive: a single line segment

layout(set = 0, binding = 0) uniform Camera
{
	mat4 Projection;
	mat4 ViewMatrix;
} uCamera;

void main() {
    
    float Length = 2.0;
    vec4 Direction = gl_in[1].gl_Position - gl_in[0].gl_Position;

    // Emit the first point (start of the line)
    gl_Position = uCamera.Projection * gl_in[0].gl_Position;
    EmitVertex();

    // Emit the second point (end of the line)
    gl_Position = uCamera.Projection * (gl_in[0].gl_Position + Direction * Length);
    EmitVertex();

    EndPrimitive(); // End this primitive (line segment)
}