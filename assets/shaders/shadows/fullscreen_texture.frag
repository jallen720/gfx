#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform sampler2D tex;

#define LIGHT_MODE_DIRECTIONAL 0
#define LIGHT_MODE_POINT 1

layout (set = 1, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
    vec3 pos;
    vec3 direction;
    int mode;
} light_ubo;

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

float linearize_depth(float depth) {
    float near_plane = 0.1;
    float far_plane = 20.0;
    return ((2.0 * near_plane * far_plane) / (far_plane + near_plane - depth * (far_plane - near_plane))) / far_plane;
}

void main() {
    float depth = texture(tex, in_uv).r;
    if (light_ubo.mode == LIGHT_MODE_POINT)
        depth = linearize_depth(depth);
    out_color = vec4(vec3(depth), 1);
}
