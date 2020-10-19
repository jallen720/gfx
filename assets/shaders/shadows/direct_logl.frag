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
} light_ubo;

layout (set = 2, binding = 0) uniform sampler2D tex;
layout (set = 3, binding = 0) uniform sampler2D shadow_map;

layout (location = 0) in vec3 in_frag_pos;
layout (location = 1) in vec3 in_frag_norm;
layout (location = 2) in vec2 in_frag_uv;
layout (location = 0) out vec4 out_color;

float LinearizeDepth(float depth, float near_plane, float far_plane) {
    float z = depth;// * 2.0 - 1.0; // Back to NDC
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

float tutorial_calc_shadow(vec3 normalized_frag_normal, vec3 normalized_light_dir, vec4 frag_pos_light_space) {
    // perform perspective divide
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    // transform to [0,1] range
    // proj_coords = proj_coords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closest_depth = texture(shadow_map, proj_coords.xy).r;
    float near_plane = 0.1;
    float far_plane = 20.0;
    if (light_ubo.mode == LIGHT_MODE_POINT)
        closest_depth = LinearizeDepth(closest_depth, near_plane, far_plane) / far_plane;
    // get depth of current fragment from light's perspective
    float curr_depth = proj_coords.z;
    // calculate bias
    float bias = max(0.05 * (1.0 - dot(normalized_frag_normal, normalized_light_dir)), 0.005);
    // check whether current frag pos is in shadow
    float shadow = curr_depth - bias > closest_depth ? 1.0 : 0.0;

    return shadow;
}

// Converts fragment's x/y coordinates from NDC-space: [-1..1] to uv-space: [0..1] for sampling shadow map.
const mat4 ndc_to_uv_mtx = mat4(0.5, 0.0, 0.0, 0.0,
                                0.0, 0.5, 0.0, 0.0,
                                0.0, 0.0, 1.0, 0.0,
                                0.5, 0.5, 0.0, 1.0);

vec3 tutorial() {
    vec3 surface_color = vec3(1.0);//texture(tex, in_frag_uv).rgb;
    vec3 light_color = vec3(1.0);
    vec4 frag_pos_light_space = ndc_to_uv_mtx * light_ubo.space_mtx * vec4(in_frag_pos, 1);
    vec3 normalized_frag_normal = normalize(in_frag_norm);
    vec3 normalized_light_dir = light_ubo.mode == LIGHT_MODE_DIRECTIONAL
                                ? normalize(-light_ubo.direction)
                                : normalize(light_ubo.pos - in_frag_pos);
    // ambient
    vec3 ambient = 0.15 * surface_color;
    // diffuse
    float diffuse_val = max(dot(normalized_light_dir, normalized_frag_normal), 0.0);
    vec3 diffuse = diffuse_val * light_color;
    // specular
    // vec3 viewDir = normalize(viewPos - in_frag_pos);
    // vec3 half_dir = normalize(normalized_light_dir + viewDir);
    // float spec_val = pow(max(dot(normalized_frag_normal, half_dir), 0.0), 64.0);
    vec3 specular = vec3(0);//spec_val * light_color;
    // calculate shadow
    float shadow = tutorial_calc_shadow(normalized_frag_normal, normalized_light_dir, frag_pos_light_space);
    return (ambient + (1.0 - shadow) * (diffuse + specular)) * surface_color;
}

void main() {
    out_color = vec4(tutorial(), 1);
}
