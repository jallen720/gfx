#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_model_ubo {
    mat4 model_mtx;
    mat4 mvp_mtx;
} model_ubo;

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 0) out vec2 out_uv;

void main() {
    gl_Position = model_ubo.mvp_mtx * vec4(in_pos, 1);
    out_uv = in_uv;
}
