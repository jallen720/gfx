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

layout (set = 1, binding = 0, std140) uniform Lights {
    Light data[16];
    uint size;
    uint count;
} lights;

layout (set = 2, binding = 0, std140) uniform Materials {
    Material data[16];
} materials;

layout (push_constant) uniform PushConstants {
    vec3 view_position;
    int mode;
} push_constants;

layout (location = 0) out vec4 out_color;

float attenuation(Light light, float light_distance) {
    // Fatt = 1.0 / (Kc + Kl ∗ d + Kq ∗ d ^ 2)
    return 1.0 / (1.0 + (light.linear * light_distance) + (light.quadratic * pow(light_distance, 2)));
}

vec3 example_blinn_phong(Fragment fragment, Light light, Material material) {
    vec3 lightColor = light.color.rgb * light.intensity;
    vec3 lightPos = light.position;

    // diffuse
    vec3 lightDir = normalize(lightPos - fragment.position);
    float diff = max(dot(lightDir, fragment.normal), 0.0);
    vec3 diffuse = diff * lightColor;
    // specular
    vec3 viewDir = normalize(push_constants.view_position - fragment.position);
    vec3 reflectDir = reflect(-lightDir, fragment.normal);
    float spec = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);
    spec = pow(max(dot(fragment.normal, halfwayDir), 0.0), material.shine_exponent);
    vec3 specular = spec * lightColor;

    return (diffuse + specular) * attenuation(light, distance(light.position, fragment.position));
}

vec4 my_blinn_phong(Fragment fragment, Light light, Material material) {
    vec4 light_color = light.color * light.intensity;
    vec3 light_direction = normalize(light.position - fragment.position);

    // Ambient
    vec4 ambient = light_color * vec4(vec3(light.ambient_intensity), 1);

    // Diffuse
    float diffuse_value = max(dot(fragment.normal, light_direction), 0.0);
    vec4 diffuse = light_color * diffuse_value;

    // Specular
    vec3 view_direction = normalize(push_constants.view_position - fragment.position);
#if 0
    // Phong
    vec3 reflect_direction = reflect(-light_direction, fragment.normal);
    float specular_value = pow(max(dot(view_direction, reflect_direction), 0.0), material.shine_exponent);
#else
    // Blinn-Phong
    vec3 half_direction = normalize(light_direction + view_direction);
    float specular_value = pow(max(dot(fragment.normal, half_direction), 0.0), material.shine_exponent);
#endif
    vec4 specular = fragment.albedo.a * light_color * specular_value;

    // Final
    return (diffuse + specular + ambient) * attenuation(light, distance(light.position, fragment.position));
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
            for (uint light_index = 0; light_index < lights.count; ++light_index) {
                Light light = lights.data[light_index];
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
    vec3 light_direction = normalize(light.position - fragment.position);

    // Ambient
    vec4 ambient = light_color * vec4(vec3(light.ambient_intensity), 1);

    // Diffuse
    float diffuse_value = max(dot(fragment.normal, light_direction), 0.0);
    vec4 diffuse = light_color * diffuse_value;

    // Specular
    vec3 view_direction = normalize(push_constants.view_position - fragment.position);
#if 0
    // Phong
    vec3 reflect_direction = reflect(-light_direction, fragment.normal);
    float specular_value = pow(max(dot(view_direction, reflect_direction), 0.0), material.shine_exponent);
#else
    // Blinn-Phong
    vec3 half_direction = normalize(light_direction + view_direction);
    float specular_value = pow(max(dot(fragment.normal, half_direction), 0.0), material.shine_exponent);
#endif
    vec4 specular = fragment.albedo.a * light_color * specular_value;

    // Final
    return (diffuse + specular + ambient) * attenuation(light, distance(light.position, fragment.position));
}
