#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 view_mtxs[6];
    vec3 pos;
    vec3 direction;
    int mode;
    vec4 color;
    int depth_bias;
    int normal_bias;
    float linear;
    float quadratic;
    float ambient;
} light_ubo;

layout (location = 0) in vec3 in_frag_pos;

void main() {
    // get distance between fragment and light source
    float lightDistance = length(in_frag_pos.xyz - light_ubo.pos);

    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / 50;

    // write this as modified depth
    gl_FragDepth = lightDistance;
}
