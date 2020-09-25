#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MODE_COMPOSITE 0
#define MODE_ALBEDO 1
#define MODE_POSITION 2
#define MODE_NORMAL 3

struct s_light {
    vec4 color;
    vec3 position;
    float linear;
    float quadratic;
    float intensity;
    float ambient_intensity;
    mat4 view_proj_mtx;
};

struct s_material {
    uint shine_exponent;
};

struct s_fragment {
    vec4 albedo;
    vec3 position;
    vec3 normal;
    uint material_index;
};

layout (set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput in_albedo;
layout (set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput in_position;
layout (set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput in_normal;
layout (set = 0, binding = 3, input_attachment_index = 3) uniform usubpassInput in_material_index;

layout (set = 1, binding = 0) uniform sampler2D shadow_map;

layout (set = 2, binding = 0, std140) uniform u_lights {
    s_light data[16];
    uint size;
    uint count;
} lights;

layout (set = 3, binding = 0, std140) uniform u_materials {
    s_material data[16];
} materials;

layout (push_constant) uniform u_push_constants {
    vec3 view_position;
    int mode;
} push_constants;

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

float attenuation(s_light light, float light_distance) {
    // Fatt = 1.0 / (Kc + Kl ∗ d + Kq ∗ d ^ 2)
    return 1.0 / (1.0 + (light.linear * light_distance) + (light.quadratic * pow(light_distance, 2)));
}

float bias(s_fragment frag, vec3 light_dir) {
    return max(0.5 * (1.0 - dot(frag.normal, light_dir)), 0.005);
}

float textureProj(s_fragment frag, s_light light, vec2 off) {
    vec4 light_space_frag_pos = light.view_proj_mtx * vec4(frag.position, 1);
    float shadow = 0.0;
    if (light_space_frag_pos.z > -1.0 && light_space_frag_pos.z < 1.0) {
        float dist = texture(shadow_map, light_space_frag_pos.st + off).r;
        if (light_space_frag_pos.w > 0.0 && dist < light_space_frag_pos.z)
            shadow = 1.0;
    }
    return shadow;
}

float ShadowCalculation(s_fragment frag, s_light light, vec3 light_dir) {
    vec4 frag_light_space_pos = light.view_proj_mtx * vec4(frag.position, 1);
    // perform perspective divide
    vec3 proj_coords = frag_light_space_pos.xyz / frag_light_space_pos.w;
    // transform to [0,1] range
    proj_coords = (proj_coords * 0.5) + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closest_depth = texture(shadow_map, proj_coords.st).r;
    // get depth of current frag from light's perspective
    float curr_depth = proj_coords.z;
    // check whether current frag pos is in shadow
    float shadow = (curr_depth - bias(frag, light_dir)) > closest_depth ? 1.0 : 0.0;

    return shadow;
}

float my_shadow(s_fragment frag, s_light light, vec3 light_dir) {
    vec4 light_space_frag_pos = light.view_proj_mtx * vec4(frag.position, 1);
    vec3 proj_frag_pos = light_space_frag_pos.xyz / light_space_frag_pos.w;
    proj_frag_pos = (proj_frag_pos * 0.5) + 0.5; // Adjust projected frag position from [-1,1] to [0,1].
    float shadow_map_depth = texture(shadow_map, proj_frag_pos.xy).r;
    float shadow_val = (proj_frag_pos.z - bias(frag, light_dir)) > shadow_map_depth ? 1.0 : 0.0;
    return shadow_val;
}

float calc_shadow(s_fragment frag, s_light light, vec3 light_dir) {
    vec4 light_space_frag_pos = light.view_proj_mtx * vec4(frag.position, 1);
    vec4 shadow_frag_pos = light_space_frag_pos / light_space_frag_pos.w;
    float shadow = 1.0;
    float shadow_frag_depth = shadow_frag_pos.z;
    if (shadow_frag_depth > -1.0 && shadow_frag_depth < 1.0) {
        // Convert fragment's x/y coordinates from NDC-space: [-1..1] to uv-space: [0..1] for sampling shadow map.
        vec2 shadow_map_coord = (shadow_frag_pos.xy * 0.5) + 0.5;
        float shadow_map_depth = texture(shadow_map, shadow_map_coord.st).r;

        if (/* shadow_frag_pos.w > 0.0 && */ shadow_frag_depth - 0.001/* bias(frag, light_dir) */ > shadow_map_depth)
            shadow = 0.0;
    }
    return shadow;
}

vec3 example_blinn_phong(s_fragment frag, s_light light, s_material mat) {
    vec3 lightColor = light.color.rgb * light.intensity;
    vec3 lightPos = light.position;

    // diff
    vec3 lightDir = normalize(lightPos - frag.position);
    float diff_val = max(dot(lightDir, frag.normal), 0.0);
    vec3 diff = diff_val * lightColor;
    // spec
    vec3 viewDir = normalize(push_constants.view_position - frag.position);
    vec3 reflectDir = reflect(-lightDir, frag.normal);
    float spec_val = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);
    spec_val = pow(max(dot(frag.normal, halfwayDir), 0.0), mat.shine_exponent);
    vec3 spec = spec_val * lightColor;

    return (diff + spec) * attenuation(light, distance(light.position, frag.position));
}

vec4 my_blinn_phong(s_fragment frag, s_light light, s_material mat) {
    vec4 light_color = light.color * light.intensity;
    vec3 light_dir = normalize(light.position - frag.position);

    // Shadow
    // float shadow_val = ShadowCalculation(frag, light, light_dir);
    // float shadow_val = my_shadow(frag, light, light_dir);
    // float shadow_val = textureProj(frag, light, vec2(0.0));
    float shadow_val = calc_shadow(frag, light, light_dir);
    // float shadow_val = 0;

    // Ambient
    vec4 ambient = light_color * vec4(vec3(light.ambient_intensity), 1);

    // Diffuse
    float diff_val = max(dot(frag.normal, light_dir), 0.0);
    vec4 diff = light_color * diff_val;

    // Specular
    vec3 view_dir = normalize(push_constants.view_position - frag.position);
#if 0
    // Phong
    vec3 reflect_dir = reflect(-light_dir, frag.normal);
    float spec_val = pow(max(dot(view_dir, reflect_dir), 0.0), mat.shine_exponent);
#else
    // Blinn-Phong
    vec3 half_dir = normalize(light_dir + view_dir);
    float spec_val = pow(max(dot(frag.normal, half_dir), 0.0), mat.shine_exponent);
#endif
    vec4 spec = light_color * spec_val * frag.albedo.a;

    // Final
    return (ambient + (shadow_val * (spec + diff))) * attenuation(light, distance(light.position, frag.position));
}

void main() {
    s_fragment frag;
    frag.albedo = vec4(subpassLoad(in_albedo).rgb, 1);
    frag.position = subpassLoad(in_position).rgb;
    frag.normal = normalize(subpassLoad(in_normal).rgb);
    frag.material_index = subpassLoad(in_material_index).r;
    switch (push_constants.mode) {
        case MODE_COMPOSITE: {
            vec4 total_light_color = vec4(0);
            s_material mat = materials.data[frag.material_index];
            for (uint i = 0; i < lights.count; ++i) {
                s_light light = lights.data[i];
                total_light_color += my_blinn_phong(frag, light, mat);
            }
            out_color = frag.albedo * total_light_color;
            break;
        }
        case MODE_ALBEDO: {
            out_color = frag.albedo;
            break;
        }
        case MODE_POSITION: {
            out_color = vec4(frag.position / 16, 1);
            out_color.y *= -1;
            break;
        }
        case MODE_NORMAL: {
            out_color = vec4(frag.normal, 1);
            out_color.y *= -1;
            break;
        }
        default: {
            out_color = vec4(0.51, 0.96, 0.17, 1);
        }
    }
}

vec4 my_blinn_phong_v0(s_fragment frag, s_light light, s_material mat) {
    vec4 light_color = light.color * light.intensity;
    vec3 light_dir = normalize(light.position - frag.position);

    // Ambient
    vec4 ambient = light_color * vec4(vec3(light.ambient_intensity), 1);

    // Diffuse
    float diff_val = max(dot(frag.normal, light_dir), 0.0);
    vec4 diff = light_color * diff_val;

    // Specular
    vec3 view_dir = normalize(push_constants.view_position - frag.position);
#if 0
    // Phong
    vec3 reflect_dir = reflect(-light_dir, frag.normal);
    float spec_val = pow(max(dot(view_dir, reflect_dir), 0.0), mat.shine_exponent);
#else
    // Blinn-Phong
    vec3 half_dir = normalize(light_dir + view_dir);
    float spec_val = pow(max(dot(frag.normal, half_dir), 0.0), mat.shine_exponent);
#endif
    vec4 spec = frag.albedo.a * light_color * spec_val;

    // Final
    return (diff + spec + ambient) * attenuation(light, distance(light.position, frag.position));
}
