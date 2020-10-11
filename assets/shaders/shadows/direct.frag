#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
    vec3 pos;
    vec3 direction;
} light_ubo;

layout (set = 2, binding = 0) uniform sampler2D tex;
layout (set = 3, binding = 0) uniform sampler2D shadow_map;

layout (location = 0) in vec3 in_frag_pos;
layout (location = 1) in vec3 in_frag_norm;
layout (location = 2) in vec2 in_frag_uv;
layout (location = 0) out vec4 out_color;

// Converts fragment's x/y coordinates from NDC-space: [-1..1] to uv-space: [0..1] for sampling shadow map.
const mat4 ndc_to_uv_mtx = mat4(0.5f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.5f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.5f, 0.5f, 0.0f, 1.0f);

// #define DIR_LIGHT
// #define POINT_LIGHT

float linearize_depth(float depth) {
#ifdef DIR_LIGHT
    return depth;
#else
    float z = depth * 2.0 - 1.0; // Back to NDC
    float near_plane = 0.01;
    float far_plane = 20.0;
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
#endif
}

float calc_shadow(vec4 shadow_frag_pos, float bias, vec2 offset) {
    float shadow = 1.0;
    float shadow_frag_depth = shadow_frag_pos.z;
    if (shadow_frag_depth > -1.0 && shadow_frag_depth < 1.0) {
        float shadow_map_depth = linearize_depth(texture(shadow_map, shadow_frag_pos.st + offset).r);
        if (shadow_frag_pos.w > 0.0 && shadow_frag_depth - bias > shadow_map_depth)
            shadow = 0.1;
    }
    return shadow;
}

float pcf_filter(vec4 shadow_frag_pos, float bias) {
    float scale = 1;
    vec2 texel_size = 1.0 / textureSize(shadow_map, 0) * scale;
    float shadow_factor = 0.0;
    int count = 0;
    int range = 4;
    for (int x = -range; x <= range; ++x)
    for (int y = -range; y <= range; ++y) {
        shadow_factor += calc_shadow(shadow_frag_pos, bias, vec2(x, y) * texel_size);
        ++count;
    }
    return shadow_factor / count;
}

float calc_shadow_bias(float max_bias, vec3 frag_norm, vec3 dir_to_light) {
    return max_bias - max(max_bias * (1.0 - dot(frag_norm, dir_to_light)), 0.0);
}

void main() {
#ifdef DIR_LIGHT
    vec3 dir_to_light = normalize(-light_ubo.direction);
#else
    vec3 dir_to_light = normalize(light_ubo.pos - in_frag_pos);
#endif
    vec3 frag_norm = normalize(in_frag_norm);
    float shadow_bias = calc_shadow_bias(0.012, frag_norm, dir_to_light);//0.05 - max(0.05 * abs(1.0 - dot(frag_norm, dir_to_light)), 0.001);
    vec4 frag_pos_light_space = ndc_to_uv_mtx * light_ubo.space_mtx * vec4(in_frag_pos, 1);
    vec4 shadow_frag_pos = frag_pos_light_space / frag_pos_light_space.w;
    float shadow = pcf_filter(shadow_frag_pos, shadow_bias);
    // float shadow = calc_shadow(shadow_frag_pos, shadow_bias, vec2(0));
    out_color = texture(tex, in_frag_uv) * shadow;
}
