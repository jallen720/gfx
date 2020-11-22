#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (push_constant) uniform u_pcs {
    vec4 color;
} pcs;

layout (location = 0) out vec4 color;

void main() {
    color = pcs.color;
}
