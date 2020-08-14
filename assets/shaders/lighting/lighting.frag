#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MODE_COMPOSITE 0
#define MODE_ALBEDO 1
#define MODE_POSITION 2
#define MODE_NORMAL 3

struct light {
    vec4  Color;
    vec4  Position;
    // vec3  direction;
    // int   mode;
    vec4 Linear;
    vec4 Quadratic;
    // float cutoff;
    // float outer_cutoff;
};

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput AlbedoInput;
layout(set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput PositionInput;
layout(set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput NormalInput;

layout(set = 1, binding = 0, std140) uniform lights {
    light Data[16];
    uint Size;
    uint Count;
} Lights;

layout(push_constant) uniform push_constants {
    int Mode;
} PushConstants;

layout(location = 0) out vec4 OutColor;

vec4 calculate_diffuse_specular(vec4 Albedo, vec3 Position, vec3 Normal, vec4 Specular, vec4 SurfaceColor, vec3 DirectionToLight) {
    // Diffuse
    float DiffuseValue = max(dot(Normal, DirectionToLight), 0.0);
    vec4 Diffuse = SurfaceColor * Albedo * DiffuseValue;

    // // Specular
    // vec3 ViewDirection = normalize(push_constants.view_position - Position);
    // vec3 ReflectDirection = reflect(-DirectionToLight, Normal);
    // float SpecularValue = pow(max(dot(ViewDirection, ReflectDirection), 0.0), material.shininess);
    // vec4 Specular = SurfaceColor * Specular * SpecularValue;

    return Diffuse/*  + Specular */;
}

float calculate_attenuation(light Light, float DistanceToLight) {
    // Fatt = 1.0 / (Kc + Kl ∗ d + Kq ∗ d^2)
    return 1 / (1.0 + (Light.Linear.x * DistanceToLight) + (Light.Quadratic.x * pow(DistanceToLight, 2)));
}

void main() {
    vec4 Albedo = subpassLoad(AlbedoInput);
    vec4 Specular = vec4(0);
    vec3 Position = subpassLoad(PositionInput).rgb;
    vec3 Normal = subpassLoad(NormalInput).rgb;
    switch(PushConstants.Mode) {
        case MODE_COMPOSITE: {
            const float AMBIENT_VALUE = 0.01;
            vec4 FinalColor = vec4(0);
            for(uint LightIndex = 0; LightIndex < Lights.Count; ++LightIndex) {
                vec4 SurfaceColor = Lights.Data[LightIndex].Color;
                vec3 DirectionToLight = normalize(Lights.Data[LightIndex].Position.xyz - Position);
                float DistanceToLight = distance(Lights.Data[LightIndex].Position.xyz, Position);
                vec4 Ambient = Lights.Data[LightIndex].Color * AMBIENT_VALUE;
                vec4 DiffuseSpecular = calculate_diffuse_specular(Albedo, Position, Normal, Specular, SurfaceColor, DirectionToLight);
                FinalColor += (DiffuseSpecular + Ambient) * calculate_attenuation(Lights.Data[LightIndex], DistanceToLight);
            }
            OutColor = FinalColor;
            break;
        }
        case MODE_ALBEDO: {
            OutColor = Albedo;
            break;
        }
        case MODE_POSITION: {
            OutColor = vec4(Position / 16, 1);
            OutColor.y *= -1;
            break;
        }
        case MODE_NORMAL: {
            OutColor = vec4(Normal, 1);
            OutColor.y *= -1;
            break;
        }
        default: {
            OutColor = vec4(0.51, 0.96, 0.17, 1);
        }
    }
}
