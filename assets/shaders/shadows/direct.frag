#version 450
#extension GL_ARB_separate_shader_objects : enable

#define LIGHT_MODE_DIRECTIONAL 0
#define LIGHT_MODE_POINT 1

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
    vec3 pos;
    vec3 direction;
    int mode;
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

float calc_shadow_bias(float max_bias, float min_bias, vec3 frag_norm, vec3 dir_to_light) {
    return max(max_bias * (1.0 - dot(frag_norm, dir_to_light)), min_bias);
}

float linearize_depth(float depth) {
    float z = depth;//(depth * 2.0) - 1.0; // Back to NDC
    float near_plane = 0.1;
    float far_plane = 20.0;
    return ((2.0 * near_plane * far_plane) / (far_plane + near_plane - (z * (far_plane - near_plane)))) / far_plane;
}

float calc_shadow_val(vec3 frag_norm, vec3 dir_to_light) {
    float shadow_bias = calc_shadow_bias(0.05, 0.005, frag_norm, dir_to_light);
    vec4 frag_pos_light_space = ndc_to_uv_mtx * light_ubo.space_mtx * vec4(in_frag_pos, 1);
    vec4 shadow_frag_pos = frag_pos_light_space / frag_pos_light_space.w;
    float scale = 1;
    vec2 texel_size = 1.0 / textureSize(shadow_map, 0) * scale;
    float shadow_factor = 0.0;
    int count = 0;
    int range = 3;
    for (int x = -range; x <= range; ++x)
    for (int y = -range; y <= range; ++y) {
        float shadow = 1.0;
        float shadow_frag_depth = shadow_frag_pos.z;
        if (shadow_frag_depth > -1.0 && shadow_frag_depth < 1.0) {
            float shadow_map_depth = texture(shadow_map, shadow_frag_pos.st + (vec2(x, y) * texel_size)).r;
            if (light_ubo.mode == LIGHT_MODE_POINT)
                shadow_map_depth = linearize_depth(shadow_map_depth);
            if (shadow_frag_pos.w > 0.0 && shadow_frag_depth - shadow_bias > shadow_map_depth)
                shadow = 0.0;
        }
        shadow_factor += shadow;
        ++count;
    }
    return shadow_factor / count;
}

float calc_shadow_val_tut(vec3 frag_norm, vec3 dir_to_light) {
    vec4 frag_pos_light_space = light_ubo.space_mtx * vec4(in_frag_pos, 1);
    // perform perspective divide
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    // transform to [0,1] range
    proj_coords = (proj_coords * 0.5) + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closest_depth = texture(shadow_map, proj_coords.xy).r;
    // get depth of current fragment from light's perspective
    float curr_depth = proj_coords.z;
    // calculate bias (based on depth map resolution and slope)
    float bias = max(0.05 * (1.0 - dot(frag_norm, dir_to_light)), 0.005);
    // check whether current frag pos is in shadow
    // float shadow = curr_depth - bias > closest_depth  ? 1.0 : 0.0;
    // PCF
    float shadow = 1.0;
    vec2 texel_size = 1.0 / textureSize(shadow_map, 0);
    for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y) {
        float pcf_depth = texture(shadow_map, proj_coords.xy + vec2(x, y) * texel_size).r;
        shadow += curr_depth - bias > pcf_depth ? 0.0 : 1.0;
    }
    shadow /= 9.0;

    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if (proj_coords.z > 1.0)
        shadow = 0.0;

    return shadow;
}

vec4 blinn_phong() {
    vec4 light_color = vec4(1);//light.color * light.intensity;
    vec3 frag_norm = normalize(in_frag_norm);
    vec3 dir_to_light = light_ubo.mode == LIGHT_MODE_DIRECTIONAL
                        ? normalize(-light_ubo.direction)
                        : normalize(light_ubo.pos - in_frag_pos);

    // Shadow
    float shadow_val = calc_shadow_val(frag_norm, dir_to_light);

    // Ambient
    vec4 ambient = vec4(light_color.rgb * 0.1, 1);

    // Diffuse
    float diff_val = max(dot(frag_norm, dir_to_light), 0.0);
    vec4 diff = light_color * diff_val;

//     // Specular
//     vec3 view_dir = normalize(push_constants.view_position - frag.position);
// #if 0
//     // Phong
//     vec3 reflect_dir = reflect(-light_dir, frag_norm);
//     float spec_val = pow(max(dot(view_dir, reflect_dir), 0.0), mat.shine_exponent);
// #else
//     // Blinn-Phong
//     vec3 half_dir = normalize(light_dir + view_dir);
//     float spec_val = pow(max(dot(frag_norm, half_dir), 0.0), mat.shine_exponent);
// #endif
//     vec4 spec = light_color * spec_val * frag.albedo.a;

    // Final
#if 1
    return ambient + (shadow_val * diff);
#else
    return (ambient + (shadow_val * (spec + diff))) * attenuation(light, distance(light.position, frag.position));
#endif
}

void main() {
    out_color = /* texture(tex, in_frag_uv) * */ blinn_phong();
}
