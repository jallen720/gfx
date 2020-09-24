#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
    vec3 pos;
} light_ubo;

layout (set = 1, binding = 0, std140) uniform u_model_ubo {
    mat4 model_mtx;
    mat4 mvp_mtx;
} model_ubo;

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 0) out vec2 out_uv;
layout (location = 1) out vec4 out_light_space_frag_pos;

void main() {
    vec4 vert_pos = vec4(in_pos, 1);
    gl_Position = model_ubo.mvp_mtx * vert_pos;
    out_light_space_frag_pos = light_ubo.space_mtx * model_ubo.model_mtx * vert_pos;
    out_uv = in_uv;
}
