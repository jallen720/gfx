#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
    vec3 pos;
} light_ubo;

layout (set = 2, binding = 0) uniform sampler2D tex;
layout (set = 3, binding = 0) uniform sampler2D shadow_map;

layout (location = 0) in vec4 in_frag_pos;
layout (location = 1) in vec3 in_frag_norm;
layout (location = 2) in vec2 in_frag_uv;
layout (location = 0) out vec4 out_color;

// Converts fragment's x/y coordinates from NDC-space: [-1..1] to uv-space: [0..1] for sampling shadow map.
const mat4 ndc_to_uv_mtx = mat4(0.5f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.5f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.5f, 0.5f, 0.0f, 1.0f);

float calc_shadow_bias(vec3 norm, vec3 light_dir, float min_bias) {
    return max(0.05 * (1.0 - dot(norm, light_dir)), min_bias);
}

float calc_shadow(vec4 shadow_frag_pos, float bias, vec2 offset) {
    float shadow = 1.0;
    float shadow_frag_depth = shadow_frag_pos.z;
    if (shadow_frag_depth > -1.0 && shadow_frag_depth < 1.0) {
        float shadow_map_depth = texture(shadow_map, shadow_frag_pos.st + offset).r;
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
    int range = 3;
    for (int x = -range; x <= range; ++x)
    for (int y = -range; y <= range; ++y) {
        shadow_factor += calc_shadow(shadow_frag_pos, bias, vec2(x, y) * texel_size);
        ++count;
    }
    return shadow_factor / count;
}

void main() {
    // float shadow_bias = calc_shadow_bias(in_frag_norm, vec3(0, 0.707, 0.707), 0.005);
    vec4 frag_pos_light_space = ndc_to_uv_mtx * light_ubo.space_mtx * in_frag_pos;
    vec4 shadow_frag_pos = frag_pos_light_space / frag_pos_light_space.w;
    // float shadow = pcf_filter(shadow_frag_pos, 0.0001);
    float shadow = calc_shadow(shadow_frag_pos, 0, vec2(0));
    out_color = texture(tex, in_frag_uv) * shadow;
}
