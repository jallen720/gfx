#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 1, binding = 0, std140) uniform u_model_ubo {
    mat4 model_mtx;
    mat4 mvp_mtx;
} model_ubo;

layout (location = 0) in vec3 in_vert_pos;

void main() {
    gl_Position = model_ubo.mvp_mtx * vec4(in_vert_pos, 1);
}
