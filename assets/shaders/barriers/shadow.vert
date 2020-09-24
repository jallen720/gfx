#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_model_ubo {
    mat4 model_mtx;
    mat4 mvp_mtx;
} model_ubo;

layout (set = 1, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
    vec3 pos;
} light_ubo;

layout (location = 0) in vec3 in_pos;

void main() {
    gl_Position = light_ubo.space_mtx * model_ubo.model_mtx * vec4(in_pos, 1);
}
