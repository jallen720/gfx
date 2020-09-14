#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MODE_COMPOSITE 0
#define MODE_ALBEDO 1
#define MODE_POSITION 2
#define MODE_NORMAL 3

struct Light {
    vec4 color;
    vec3 position;
    float linear;
    float quadratic;
    float intensity;
    float ambient_intensity;
    mat4 view_proj_mtx;
};

struct Material {
    uint shine_exponent;
};

struct Fragment {
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

layout (set = 2, binding = 0, std140) uniform Lights {
    Light data[16];
    uint size;
    uint count;
} lights;

layout (set = 3, binding = 0, std140) uniform Materials {
    Material data[16];
} materials;

layout (push_constant) uniform PushConstants {
    vec3 view_position;
    int mode;
} push_constants;

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

float attenuation(Light light, float light_distance) {
    // Fatt = 1.0 / (Kc + Kl ∗ d + Kq ∗ d ^ 2)
    return 1.0 / (1.0 + (light.linear * light_distance) + (light.quadratic * pow(light_distance, 2)));
}

float bias(Fragment fragment, vec3 light_dir) {
    return max(0.5 * (1.0 - dot(fragment.normal, light_dir)), 0.005);
}

float textureProj(Fragment fragment, Light light, vec2 off) {
    vec4 light_space_frag_pos = light.view_proj_mtx * vec4(fragment.position, 1);
    float shadow = 0.0;
    if (light_space_frag_pos.z > -1.0 && light_space_frag_pos.z < 1.0) {
        float dist = texture(shadow_map, light_space_frag_pos.st + off).r;
        if (light_space_frag_pos.w > 0.0 && dist < light_space_frag_pos.z)
            shadow = 1.0;
    }
    return shadow;
}

float ShadowCalculation(Fragment fragment, Light light, vec3 light_dir) {
    vec4 fragPosLightSpace = light.view_proj_mtx * vec4(fragment.position, 1);
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = (projCoords * 0.5) + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadow_map, projCoords.xy).r;
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    float shadow = (currentDepth - bias(fragment, light_dir)) > closestDepth ? 1.0 : 0.0;

    return shadow;
}

float my_shadow(Fragment fragment, Light light, vec3 light_dir) {
    vec4 light_space_frag_pos = light.view_proj_mtx * vec4(fragment.position, 1);
    vec3 proj_frag_pos = light_space_frag_pos.xyz / light_space_frag_pos.w;
    proj_frag_pos = (proj_frag_pos * 0.5) + 0.5; // Adjust projected frag position from [-1,1] to [0,1].
    float shadow_map_depth = texture(shadow_map, proj_frag_pos.xy).r;
    float shadow_val = (proj_frag_pos.z - bias(fragment, light_dir)) > shadow_map_depth ? 1.0 : 0.0;
    return shadow_val;
}

vec3 example_blinn_phong(Fragment fragment, Light light, Material material) {
    vec3 lightColor = light.color.rgb * light.intensity;
    vec3 lightPos = light.position;

    // diff
    vec3 lightDir = normalize(lightPos - fragment.position);
    float diff_val = max(dot(lightDir, fragment.normal), 0.0);
    vec3 diff = diff_val * lightColor;
    // spec
    vec3 viewDir = normalize(push_constants.view_position - fragment.position);
    vec3 reflectDir = reflect(-lightDir, fragment.normal);
    float spec_val = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);
    spec_val = pow(max(dot(fragment.normal, halfwayDir), 0.0), material.shine_exponent);
    vec3 spec = spec_val * lightColor;

    return (diff + spec) * attenuation(light, distance(light.position, fragment.position));
}

vec4 my_blinn_phong(Fragment fragment, Light light, Material material) {
    vec4 light_color = light.color * light.intensity;
    vec3 light_dir = normalize(light.position - fragment.position);

    // Shadow
    float shadow_val = ShadowCalculation(fragment, light, light_dir);
    // float shadow_val = my_shadow(fragment, light, light_dir);
    // float shadow_val = textureProj(fragment, light, vec2(0.0));
    // float shadow_val = 0;

    // Ambient
    vec4 ambient = light_color * vec4(vec3(light.ambient_intensity), 1);

    // Diffuse
    float diff_val = max(dot(fragment.normal, light_dir), 0.0);
    vec4 diff = light_color * diff_val;

    // Specular
    vec3 view_dir = normalize(push_constants.view_position - fragment.position);
#if 0
    // Phong
    vec3 reflect_dir = reflect(-light_dir, fragment.normal);
    float spec_val = pow(max(dot(view_dir, reflect_dir), 0.0), material.shine_exponent);
#else
    // Blinn-Phong
    vec3 half_dir = normalize(light_dir + view_dir);
    float spec_val = pow(max(dot(fragment.normal, half_dir), 0.0), material.shine_exponent);
#endif
    vec4 spec = light_color * spec_val * fragment.albedo.a;

    // Final
    return (ambient + ((1.0 - shadow_val) * (spec + diff))) * attenuation(light, distance(light.position, fragment.position));
}

void main() {
    Fragment fragment;
    fragment.albedo = vec4(subpassLoad(in_albedo).rgb, 1);
    fragment.position = subpassLoad(in_position).rgb;
    fragment.normal = normalize(subpassLoad(in_normal).rgb);
    fragment.material_index = subpassLoad(in_material_index).r;
    switch (push_constants.mode) {
        case MODE_COMPOSITE: {
            vec4 total_light_color = vec4(0);
            Material material = materials.data[fragment.material_index];
            for (uint i = 0; i < lights.count; ++i) {
                Light light = lights.data[i];
                total_light_color += my_blinn_phong(fragment, light, material);
            }
            out_color = fragment.albedo * total_light_color;
            break;
        }
        case MODE_ALBEDO: {
            out_color = fragment.albedo;
            break;
        }
        case MODE_POSITION: {
            out_color = vec4(fragment.position / 16, 1);
            out_color.y *= -1;
            break;
        }
        case MODE_NORMAL: {
            out_color = vec4(fragment.normal, 1);
            out_color.y *= -1;
            break;
        }
        default: {
            out_color = vec4(0.51, 0.96, 0.17, 1);
        }
    }
}

vec4 my_blinn_phong_v0(Fragment fragment, Light light, Material material) {
    vec4 light_color = light.color * light.intensity;
    vec3 light_dir = normalize(light.position - fragment.position);

    // Ambient
    vec4 ambient = light_color * vec4(vec3(light.ambient_intensity), 1);

    // Diffuse
    float diff_val = max(dot(fragment.normal, light_dir), 0.0);
    vec4 diff = light_color * diff_val;

    // Specular
    vec3 view_dir = normalize(push_constants.view_position - fragment.position);
#if 0
    // Phong
    vec3 reflect_dir = reflect(-light_dir, fragment.normal);
    float spec_val = pow(max(dot(view_dir, reflect_dir), 0.0), material.shine_exponent);
#else
    // Blinn-Phong
    vec3 half_dir = normalize(light_dir + view_dir);
    float spec_val = pow(max(dot(fragment.normal, half_dir), 0.0), material.shine_exponent);
#endif
    vec4 spec = fragment.albedo.a * light_color * spec_val;

    // Final
    return (diff + spec + ambient) * attenuation(light, distance(light.position, fragment.position));
}
