#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    vec4 color;
    vec3 pos;
    int mode;
    vec3 dir;
    float far_clip;
    int depth_bias;
    int normal_bias;
    float linear;
    float quadratic;
    float ambient;
} light_ubo;

layout (location = 0) out vec4 color;

void main() {
    color = light_ubo.color;
}
