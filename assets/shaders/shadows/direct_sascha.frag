#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 2, binding = 0) uniform sampler2D tex;
layout (set = 3, binding = 0) uniform sampler2D shadow_map;

layout (location = 0) in vec4 in_frag_pos_light_space;
layout (location = 1) in vec3 in_frag_norm;
layout (location = 2) in vec2 in_frag_uv;
layout (location = 3) in vec3 in_frag_light_dir;

layout (location = 0) out vec4 out_color;

#define AMBIENT 0.1

float calc_shadow(vec4 frag_pos_light_space) {
    float shadow = 1.0;
    if (frag_pos_light_space.z > -1.0 && frag_pos_light_space.z < 1.0) {
        float shadow_depth = texture(shadow_map, frag_pos_light_space.st).r;
        if (frag_pos_light_space.w > 0.0 && shadow_depth < frag_pos_light_space.z - 0.001)
            shadow = 0.0;
    }
    return shadow;
}

void main() {
    vec4 frag_pos_light_space = in_frag_pos_light_space / in_frag_pos_light_space.w;
    vec3 frag_norm = normalize(in_frag_norm);
    vec3 frag_light_dir = normalize(in_frag_light_dir);

    vec3 surface_color = texture(tex, in_frag_uv).rgb;
    vec4 ambient = vec4(AMBIENT) * vec4(surface_color, 1);
    float shadow = calc_shadow(frag_pos_light_space);
    vec4 diffuse = max(dot(frag_norm, frag_light_dir), 0.0) * vec4(surface_color, 1);
    out_color = (ambient + (shadow * diffuse));
}
