#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_entity_mtxs {
    mat4 model;
    mat4 mvp;
} entity_mtxs;

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 0) out vec2 out_uv;

void main() {
    gl_Position = entity_mtxs.mvp * vec4(in_pos, 1);
    out_uv = in_uv;
}
