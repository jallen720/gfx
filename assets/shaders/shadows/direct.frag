#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MAX_OMNI_LIGHTS 8

struct s_omni_light_ubo {
    vec4 color;
    vec3 pos;
    int depth_bias;
    int normal_bias;
    float far_clip;
    float linear;
    float quadratic;
    float ambient;
    bool on;
};

layout (set = 0, binding = 0, std140) uniform u_lights {
    s_omni_light_ubo omni_light_ubos[MAX_OMNI_LIGHTS];
    uint count;
} lights;
layout (set = 1, binding = 0) uniform samplerCube shadow_map_3d[MAX_OMNI_LIGHTS];
layout (set = 3, binding = 0) uniform sampler2D tex;
layout (push_constant) uniform u_push_constants {
    vec3 view_pos;
} push_constants;

layout (location = 0) in vec3 in_frag_pos;
// layout (location = 1) in vec4 in_frag_pos_light_space;
layout (location = 1) in vec3 in_frag_norm;
layout (location = 2) in vec2 in_frag_uv;
// layout (location = 3) in vec3 in_frag_light_dir;

layout (location = 0) out vec4 out_color;

float calc_shadow(s_omni_light_ubo omni_light_ubo, samplerCube shadow_map_3d, vec3 biased_frag_pos, float depth_bias) {
    float shadow = 1.0;
    vec3 light_to_frag = biased_frag_pos - omni_light_ubo.pos;
    float frag_depth = length(light_to_frag);
#if 1
    float samples = 4.0;
    float offset = 0.1;
    for (float x = -offset; x < offset; x += offset / (samples * 0.5))
    for (float y = -offset; y < offset; y += offset / (samples * 0.5))
    for (float z = -offset; z < offset; z += offset / (samples * 0.5)) {
        float shadow_depth = texture(shadow_map_3d, vec3(light_to_frag.x, -light_to_frag.y, light_to_frag.z) + vec3(x, y, z)).r;
        if (frag_depth - (depth_bias * omni_light_ubo.far_clip) > (shadow_depth * omni_light_ubo.far_clip))
            shadow += 1.0;
    }
    shadow /= (samples * samples * samples);
    return 1 - shadow;
#else
    float shadow_depth = texture(shadow_map_3d, vec3(light_to_frag.x, -light_to_frag.y, light_to_frag.z)).r;
    if (frag_depth - (depth_bias * omni_light_ubo.far_clip) > (shadow_depth * omni_light_ubo.far_clip))
        shadow = 0.0;
    return shadow;
#endif
}

float depth_bias_scale(float frag_depth) {
    return 1 - frag_depth;
}

float calc_attenuation(s_omni_light_ubo omni_light_ubo, float light_dist) {
    // Fatt = 1.0 / (Kc + Kl ∗ d + Kq ∗ d ^ 2)
    return 1.0 / (1.0 + (omni_light_ubo.linear * light_dist) + (omni_light_ubo.quadratic * pow(light_dist, 2)));
}

void main() {
    vec3 frag_norm = normalize(in_frag_norm);
    float texel_size = 1 / length(textureSize(shadow_map_3d[0], 0));
#if 0
    vec4 surface_color = texture(tex, in_frag_uv);
#else
    vec4 surface_color = vec4(1);
#endif
    vec3 light_color = vec3(0);
    uint light_count = 0;
    for (uint light_idx = 0; light_idx < lights.count; ++light_idx) {
        s_omni_light_ubo omni_light_ubo = lights.omni_light_ubos[light_idx];
        if (!omni_light_ubo.on)
            continue;
        ++light_count;
        vec3 frag_norm_bias = frag_norm * omni_light_ubo.normal_bias * 0.001;
        vec3 biased_frag_pos = in_frag_pos + frag_norm_bias;
        vec3 frag_light_dir = normalize(omni_light_ubo.pos - in_frag_pos);
        float depth_bias = omni_light_ubo.depth_bias * texel_size;

        // Light Calculations
        float diffuse = max(dot(frag_norm, frag_light_dir), 0.0);
        float shadow = calc_shadow(omni_light_ubo, shadow_map_3d[light_idx], biased_frag_pos, depth_bias);
        float attenuation = calc_attenuation(omni_light_ubo, distance(in_frag_pos, omni_light_ubo.pos));
        vec4 omni_light_color = (omni_light_ubo.ambient + (shadow * diffuse)) * omni_light_ubo.color * attenuation;
        light_color += vec3(omni_light_color);
    }
    out_color = surface_color * vec4(light_color/*  / light_count */, 1);
}

// Old

    // // float texel_size = 1 / length(textureSize(shadow_map_2d, 0));
    // float texel_size = 1 / length(textureSize(shadow_map_3d, 0));
    // vec3 frag_norm = normalize(in_frag_norm);
    // vec3 frag_light_dir = normalize(in_frag_light_dir);
    // // vec4 frag_pos_light_space = in_frag_pos_light_space / in_frag_pos_light_space.w;
    // // float bias_scale = depth_bias_scale(abs(frag_pos_light_space.z));
    // float depth_bias = omni_light_ubo.depth_bias * texel_size;// * bias_scale;

    // // Light Calculations
    // float diffuse = max(dot(frag_norm, frag_light_dir), 0.0);
    // // float shadow = pcf_filter(frag_pos_light_space, depth_bias);
    // float shadow = calc_shadow(/* frag_pos_light_space,  */depth_bias, vec2(0));
    // // return;
    // float attenuation = omni_light_ubo.mode == LIGHT_MODE_DIRECTIONAL ? 1 : calc_attenuation(distance(in_frag_pos, omni_light_ubo.pos));
    // vec4 light_color = omni_light_ubo.color * (omni_light_ubo.ambient + (shadow * diffuse)) * attenuation;
    // vec4 surface_color = texture(tex, in_frag_uv);
    // out_color = surface_color * light_color;


// float calc_shadow(/* vec4 frag_pos_light_space,  */float depth_bias, vec2 offset) {
//     float shadow = 1.0;
//     // float frag_depth = frag_pos_light_space.z;
//     vec3 light_to_frag = in_frag_pos - omni_light_ubo.pos;
//     float frag_depth = length(light_to_frag) / 50;
//     // if (abs(frag_depth) < 1) {
//         // float shadow_depth = texture(shadow_map_2d, frag_pos_light_space.st + offset).r;
//         float shadow_depth = texture(shadow_map_3d, vec3(light_to_frag.x, -light_to_frag.y, light_to_frag.z)).r;
//         if (/* frag_pos_light_space.w > 0.0 && */ shadow_depth < frag_depth - depth_bias)
//             shadow = 0.0;
//     // }
//     // out_color = vec4(linearize_depth(texture(shadow_map_3d, vec3(light_to_frag.x, -light_to_frag.y, light_to_frag.z)).r));
//     return shadow;
// }

// float pcf_filter(vec4 frag_pos_light_space, float depth_bias) {
//     // ivec2 tex_dim = textureSize(shadow_map_2d, 0);
//     ivec2 tex_dim = textureSize(shadow_map_3d, 0);
//     float texel_width = 1 / float(tex_dim.x);
//     float texel_height = 1 / float(tex_dim.y);
//     float shadow = 0.0;
//     int count = 0;
//     int range = 3;
//     for (int x = -range; x <= range; x++)
//     for (int y = -range; y <= range; y++) {
//         shadow += calc_shadow(frag_pos_light_space, depth_bias, vec2(x * texel_width, y * texel_height));
//         count++;
//     }
//     return shadow / count;
// }
