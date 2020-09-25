#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 2, binding = 0) uniform sampler2D tex;
layout (set = 3, binding = 0) uniform sampler2D shadow_map;

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec4 in_light_space_frag_pos;
layout (location = 0) out vec4 out_color;
const float BIAS = 0.005;

// float shadow_bias() {
//     return max(0.05 * (1.0 - dot(normal, light_dir)), 0.005);
// }

float calc_shadow(vec4 shadow_frag_pos, vec2 offset) {
    float shadow = 1.0;
    float shadow_frag_depth = shadow_frag_pos.z;
    if (shadow_frag_depth > -1.0 && shadow_frag_depth < 1.0) {
        // Convert fragment's x/y coordinates from NDC-space: [-1..1] to uv-space: [0..1] for sampling shadow map.
        vec2 shadow_map_coord = (shadow_frag_pos.xy * 0.5) + 0.5;
        float shadow_map_depth = texture(shadow_map, shadow_map_coord.st + offset).r;
        if (shadow_frag_pos.w > 0.0 && shadow_frag_depth - BIAS > shadow_map_depth)
            shadow = 0.1;
    }
    return shadow;
}

float pcf_filter(vec4 shadow_frag_pos) {
    float scale = 1;
    vec2 texel_size = 1.0 / textureSize(shadow_map, 0) * scale;
    float shadow_factor = 0.0;
    int count = 0;
    int range = 1;
    for (int x = -range; x <= range; ++x) {
        for (int y = -range; y <= range; ++y) {
            shadow_factor += calc_shadow(shadow_frag_pos, vec2(x, y) * texel_size);
            ++count;
        }
    }
    return shadow_factor / count;
}

void main() {
    vec4 shadow_frag_pos = in_light_space_frag_pos / in_light_space_frag_pos.w;
    float shadow = pcf_filter(shadow_frag_pos);
    // float shadow = calc_shadow(shadow_frag_pos, vec2(0));
    out_color = texture(tex, in_uv) * shadow;
}
