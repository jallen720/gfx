#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 1, binding = 0, std140) uniform u_model_ubo {
    mat4 model_mtx;
    mat4 mvp_mtx;
} model_ubo;

layout (location = 0) in vec3 in_vert_pos;
layout (location = 1) in vec3 in_vert_norm;
layout (location = 2) in vec2 in_vert_uv;
layout (location = 0) out vec4 out_frag_pos;
layout (location = 1) out vec3 out_frag_norm;
layout (location = 2) out vec2 out_frag_uv;

void main() {
    vec4 vert_pos = vec4(in_vert_pos, 1);
    gl_Position = model_ubo.mvp_mtx * vert_pos;
    out_frag_pos = model_ubo.model_mtx * vert_pos;
    out_frag_norm = mat3(model_ubo.model_mtx) * in_vert_norm;
    out_frag_uv = in_vert_uv;
}
