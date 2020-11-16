#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (push_constant) uniform u_pcs {
    mat4 light_mtx;
    vec3 light_pos;
    float far_clip;
} pcs;

layout (location = 0) in vec4 in_frag_pos;

void main() {
   gl_FragDepth = length(in_frag_pos.xyz - pcs.light_pos) / pcs.far_clip;
}
