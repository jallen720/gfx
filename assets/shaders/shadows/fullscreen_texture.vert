#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 0) out vec2 out_uv;

void main() {
    gl_Position = vec4(in_pos, 1);
    out_uv = in_uv;
}
