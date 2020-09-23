#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_model_ubo {
    mat4 model_mtx;
    mat4 mvp_mtx;
} model_ubo;

// layout (set = 1, binding = 0, std140) uniform u_light_space_mtx {
//     mat4 data;
// } light_space_mtx;

layout (location = 0) in vec3 in_pos;

void main() {
    gl_Position = /* light_space_mtx.data * */ model_ubo.mvp_mtx * vec4(in_pos, 1);
}
