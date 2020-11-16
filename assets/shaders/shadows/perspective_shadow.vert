#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform u_model {
    mat4 mtx;
    mat4 mvp_mtx;
} model;
layout (push_constant) uniform u_pcs {
    mat4 light_mtx;
    vec3 light_pos;
    float far_clip;
} pcs;

layout (location = 0) in vec3 in_vert_pos;
layout (location = 0) out vec4 out_frag_pos;

void main() {
    out_frag_pos = model.mtx * vec4(in_vert_pos, 1);
    gl_Position = pcs.light_mtx * out_frag_pos;
}
