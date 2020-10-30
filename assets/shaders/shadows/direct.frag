#version 450
#extension GL_ARB_separate_shader_objects : enable

#define LIGHT_MODE_DIRECTIONAL 0
#define LIGHT_MODE_POINT 1

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
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
layout (set = 2, binding = 0) uniform sampler2D tex;
layout (set = 3, binding = 0) uniform sampler2D shadow_map;

layout (location = 0) in vec3 in_frag_pos;
layout (location = 1) in vec4 in_frag_pos_light_space;
layout (location = 2) in vec3 in_frag_norm;
layout (location = 3) in vec2 in_frag_uv;
layout (location = 4) in vec3 in_frag_light_dir;

layout (location = 0) out vec4 out_color;

float calc_shadow(vec4 frag_pos_light_space, float depth_bias, vec2 offset) {
    float shadow = 1.0;
    float frag_depth = frag_pos_light_space.z;
    if (abs(frag_depth) <= 1) {
        float shadow_depth = texture(shadow_map, frag_pos_light_space.st + offset).r;
        if (shadow_depth < frag_depth - depth_bias)
            shadow = 0.0;
    }
    return shadow;
}

float pcf_filter(vec4 frag_pos_light_space, float depth_bias) {
    ivec2 tex_dim = textureSize(shadow_map, 0);
    float texel_width = 1 / float(tex_dim.x);
    float texel_height = 1 / float(tex_dim.y);
    float shadow = 0.0;
    int count = 0;
    int range = 3;
    for (int x = -range; x <= range; x++)
    for (int y = -range; y <= range; y++) {
        shadow += calc_shadow(frag_pos_light_space, depth_bias, vec2(x * texel_width, y * texel_height));
        count++;
    }
    return shadow / count;
}

float depth_bias_scale(float frag_depth) {
    return 1 - frag_depth;
}

float calc_attenuation(float light_distance) {
    // Fatt = 1.0 / (Kc + Kl ∗ d + Kq ∗ d ^ 2)
    return 1.0 / (1.0 + (light_ubo.linear * light_distance) + (light_ubo.quadratic * pow(light_distance, 2)));
}

void main() {
    float texel_size = 1 / length(textureSize(shadow_map, 0));
    vec3 frag_norm = normalize(in_frag_norm);
    vec3 frag_light_dir = normalize(in_frag_light_dir);
    vec4 frag_pos_light_space = in_frag_pos_light_space / in_frag_pos_light_space.w;
    float bias_scale = depth_bias_scale(frag_pos_light_space.z);
    float depth_bias = light_ubo.depth_bias * texel_size * bias_scale;

    // Light Calculations
    float diffuse = max(dot(frag_norm, frag_light_dir), 0.0);
    float shadow = pcf_filter(frag_pos_light_space, depth_bias);
    float attenuation = calc_attenuation(distance(in_frag_pos, light_ubo.pos));
    vec4 light_color = light_ubo.color * (light_ubo.ambient + (shadow * diffuse)) * attenuation;
    vec4 surface_color = texture(tex, in_frag_uv);
    out_color = surface_color * light_color;
}
