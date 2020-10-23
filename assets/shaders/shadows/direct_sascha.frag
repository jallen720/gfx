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
} light_ubo;

layout (set = 2, binding = 0) uniform sampler2D tex;
layout (set = 3, binding = 0) uniform sampler2D shadow_map;

layout (location = 0) in vec4 in_frag_pos_light_space;
layout (location = 1) in vec3 in_frag_norm;
layout (location = 2) in vec2 in_frag_uv;
layout (location = 3) in vec3 in_frag_light_dir;

layout (location = 0) out vec4 out_color;

#define AMBIENT 0.3

float calc_shadow(vec4 frag_pos_light_space, float depth_bias, vec2 offset) {
    float shadow = 1.0;
    float frag_depth = frag_pos_light_space.z;
    // if (frag_depth > -1.0 && frag_depth < 1.0) {
        float shadow_depth = texture(shadow_map, frag_pos_light_space.st + offset).r;
        if (frag_pos_light_space.w > 0.0 && shadow_depth < frag_depth - depth_bias)
            shadow = 0.0;
    // }
    return shadow;
}

float pcf_filter(vec4 frag_pos_light_space, float depth_bias) {
    ivec2 tex_dim = textureSize(shadow_map, 0);
    float texel_width = 1 / float(tex_dim.x);
    float texel_height = 1 / float(tex_dim.y);
    float shadow = 0.0;
    int count = 0;
    int range = 8;
    for (int x = -range; x <= range; x++)
    for (int y = -range; y <= range; y++) {
        shadow += calc_shadow(frag_pos_light_space, depth_bias, vec2(x * texel_width, y * texel_height));
        count++;
    }
    return shadow / count;
}

void main() {
    float texel_size = 1 / length(textureSize(shadow_map, 0));
    vec3 frag_norm = normalize(in_frag_norm);
    vec3 frag_light_dir = normalize(in_frag_light_dir);
    vec4 frag_norm_bias = vec4(frag_norm * light_ubo.normal_bias * texel_size, 0);
    vec4 frag_pos_light_space = (in_frag_pos_light_space / in_frag_pos_light_space.w) + frag_norm_bias;
    float depth_bias = light_ubo.depth_bias * texel_size;

    vec3 surface_color = texture(tex, in_frag_uv).rgb;
    vec4 ambient = AMBIENT * vec4(0.9, 0.9, 1, 1) * vec4(surface_color, 1);
    float shadow = pcf_filter(frag_pos_light_space, depth_bias);
    vec4 diffuse = max(dot(frag_norm, frag_light_dir), 0.0) * vec4(surface_color, 1);
    out_color = (ambient + (shadow * diffuse));
    // out_color = vec4(bias);
}
