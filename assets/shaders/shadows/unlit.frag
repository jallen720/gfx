#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
    vec3 pos;
    vec3 direction;
    int mode;
    vec4 color;
} light_ubo;

layout (location = 0) out vec4 color;

void main() {
    color = light_ubo.color;
}
