#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MODE_COMPOSITE 0
#define MODE_ALBEDO 1
#define MODE_POSITION 2
#define MODE_NORMAL 3

struct s_light {
    vec4 color;
    // vec3 dir;
    // int mode;
    vec3 position;
    float linear;
    float quadratic;
    float intensity;
    // float cutoff;
    // float outer_cutoff;
};

struct s_material {
    uint shine_exponent;
    float specular_intensity;
};

struct s_fragment {
    vec4 albedo;
    vec3 position;
    vec3 normal;
    uint mat_idx;
};

layout (set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput in_albedo;
layout (set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput in_position;
layout (set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput in_normal;
layout (set = 0, binding = 3, input_attachment_index = 3) uniform usubpassInput in_mat_idx;

layout (set = 1, binding = 0, std140) uniform u_lights {
    s_light data[16];
    uint size;
    uint count;
} lights;

layout (set = 2, binding = 0, std140) uniform u_materials {
    s_material data[16];
} materials;

layout (push_constant) uniform push_consts_u {
    vec3 view_pos;
    int mode;
} push_consts;

layout (location = 0) out vec4 out_color;

float attenuation(s_light light, float dist_to_light) {
    // Fatt = 1.0 / (Kc + Kl∗d + Kq∗d^2)
    return 1 / (1.0 + (light.linear * dist_to_light) + (light.quadratic * pow(dist_to_light, 2)));
}

void main() {
    s_fragment frag;
    frag.albedo = vec4(subpassLoad(in_albedo).rgb, 1);
    frag.position = subpassLoad(in_position).rgb;
    frag.normal = subpassLoad(in_normal).rgb;
    frag.mat_idx = subpassLoad(in_mat_idx).r;
    switch(push_consts.mode) {
        case MODE_COMPOSITE: {
            vec4 final_color = vec4(0);
            s_material mat = materials.data[frag.mat_idx];
            for(uint LightIndex = 0; LightIndex < lights.count; ++LightIndex) {
                s_light light = lights.data[LightIndex];
                vec4 light_color = light.color * light.intensity;
                vec3 light_dir = normalize(light.position.xyz - frag.position);

                ////////////////////////////////////////////////////////////
                /// Ambient
                ////////////////////////////////////////////////////////////
                vec4 ambient = frag.albedo * light_color * vec4(vec3(0.2), 1);

                ////////////////////////////////////////////////////////////
                /// Diffuse
                ////////////////////////////////////////////////////////////
                float diffuse_val = max(dot(frag.normal, light_dir), 0.0);
                vec4 diffuse = frag.albedo * light_color * diffuse_val;

                ////////////////////////////////////////////////////////////
                /// Specular
                ////////////////////////////////////////////////////////////
                vec3 view_dir = normalize(push_consts.view_pos - frag.position);
#if 0
                // Phong
                vec3 reflect_dir = reflect(-light_dir, frag.normal);
                float specular_val = pow(max(dot(view_dir, reflect_dir), 0.0), mat.shine_exponent);
#else
                // Blinn-Phong
                vec3 half_dir = normalize(light_dir + view_dir);
                float specular_val = pow(max(dot(frag.normal, half_dir), 0.0), mat.shine_exponent);
#endif
                vec4 specular = frag.albedo.a * light_color * specular_val * mat.specular_intensity;

                ////////////////////////////////////////////////////////////
                /// Final
                ////////////////////////////////////////////////////////////
                final_color += (diffuse + specular + ambient) * attenuation(light, distance(light.position, frag.position));
            }
            out_color = final_color;
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
