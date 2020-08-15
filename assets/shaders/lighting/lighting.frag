#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MODE_COMPOSITE 0
#define MODE_ALBEDO 1
#define MODE_POSITION 2
#define MODE_NORMAL 3

struct light {
    vec4 Color;
    vec3 Position;
    float AmbientIntensity;
    // vec3 Direction;
    // int Mode;
    float Linear;
    float Quadratic;
    // float Cutoff;
    // float OuterCutoff;
};

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput AlbedoInput;
layout(set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput PositionInput;
layout(set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput NormalInput;

layout(set = 1, binding = 0, std140) uniform lights {
    light Data[16];
    uint Size;
    uint Count;
} Lights;

// layout(set = 2, binding = 0, std140) uniform material {
//     uint Shininess;
// } Material;

layout(push_constant) uniform push_constants {
    vec3 ViewPosition;
    int Mode;
} PushConstants;

layout(location = 0) out vec4 OutColor;

float attenuation(light Light, float DistanceToLight) {
    // Fatt = 1.0 / (Kc + Kl ∗ d + Kq ∗ d^2)
    return 1 / (1.0 + (Light.Linear.x * DistanceToLight) + (Light.Quadratic.x * pow(DistanceToLight, 2)));
}

void main() {
    vec4 FragmentAlbedo = subpassLoad(AlbedoInput);
    vec3 FragmentPosition = subpassLoad(PositionInput).rgb;
    vec3 FragmentNormal = subpassLoad(NormalInput).rgb;
    switch(PushConstants.Mode) {
        case MODE_COMPOSITE: {
            vec4 FinalColor = vec4(0);
            for(uint LightIndex = 0; LightIndex < Lights.Count; ++LightIndex) {
                light Light = Lights.Data[LightIndex];
                vec3 DirectionToLight = normalize(Light.Position.xyz - FragmentPosition);

                // Ambient
                vec4 Ambient = Light.Color * Light.AmbientIntensity * FragmentAlbedo.a;

                // Diffuse
                float DiffuseValue = max(dot(FragmentNormal, DirectionToLight), 0.0);
                vec4 Diffuse = Light.Color * FragmentAlbedo * DiffuseValue;

                // Specular
                vec3 ViewDirection = normalize(PushConstants.ViewPosition - FragmentPosition);
                vec3 ReflectDirection = reflect(-DirectionToLight, FragmentNormal);
                float SpecularValue = pow(max(dot(ViewDirection, ReflectDirection), 0.0), 32);//Material.Shininess);
                vec4 Specular = Light.Color * SpecularValue;

                FinalColor += (Diffuse + Specular + Ambient) * attenuation(Light, distance(Light.Position, FragmentPosition));
            }
            OutColor = FinalColor;
            break;
        }
        case MODE_ALBEDO: {
            OutColor = FragmentAlbedo;
            break;
        }
        case MODE_POSITION: {
            OutColor = vec4(FragmentPosition / 16, 1);
            OutColor.y *= -1;
            break;
        }
        case MODE_NORMAL: {
            OutColor = vec4(FragmentNormal, 1);
            OutColor.y *= -1;
            break;
        }
        default: {
            OutColor = vec4(0.51, 0.96, 0.17, 1);
        }
    }
}
