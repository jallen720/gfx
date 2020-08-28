#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MODE_COMPOSITE 0
#define MODE_ALBEDO 1
#define MODE_POSITION 2
#define MODE_NORMAL 3

struct light {
    vec4 Color;
    // vec3 Direction;
    // int Mode;
    vec3 Position;
    float Linear;
    float Quadratic;
    float Intensity;
    // float Cutoff;
    // float OuterCutoff;
};

struct material {
    uint ShineExponent;
};

struct fragment {
    vec4 Albedo;
    vec3 Position;
    vec3 Normal;
    uint MaterialIndex;
};

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput AlbedoInput;
layout(set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput PositionInput;
layout(set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput NormalInput;
layout(set = 0, binding = 3, input_attachment_index = 3) uniform usubpassInput MaterialIndexInput;

layout(set = 1, binding = 0, std140) uniform lights {
    light Data[16];
    uint Size;
    uint Count;
} Lights;

layout(set = 2, binding = 0, std140) uniform materials {
    material Data[16];
} Materials;

layout(push_constant) uniform push_constants {
    vec3 ViewPosition;
    int Mode;
} PushConstants;

layout(location = 0) out vec4 OutColor;

float attenuation(light Light, float DistanceToLight) {
    // Fatt = 1.0 / (Kc + Kl ∗ d + Kq ∗ d^2)
    return 1 / (1.0 + (Light.Linear * DistanceToLight) + (Light.Quadratic * pow(DistanceToLight, 2)));
}

void main() {
    fragment Fragment;
    Fragment.Albedo = vec4(subpassLoad(AlbedoInput).rgb, 1);
    Fragment.Position = subpassLoad(PositionInput).rgb;
    Fragment.Normal = subpassLoad(NormalInput).rgb;
    Fragment.MaterialIndex = subpassLoad(MaterialIndexInput).r;
    switch(PushConstants.Mode) {
        case MODE_COMPOSITE: {
            vec4 FinalColor = vec4(0);
            material Material = Materials.Data[Fragment.MaterialIndex];
            for(uint LightIndex = 0; LightIndex < Lights.Count; ++LightIndex) {
                light Light = Lights.Data[LightIndex];
                vec4 LightColor = Light.Color * Light.Intensity;
                vec3 DirectionToLight = normalize(Light.Position.xyz - Fragment.Position);

                // Diffuse
                float DiffuseValue = max(dot(Fragment.Normal, DirectionToLight), 0.0);
                vec4 Diffuse = Fragment.Albedo * LightColor * DiffuseValue;

                // Specular
                vec3 ViewDirection = normalize(PushConstants.ViewPosition - Fragment.Position);
                vec3 ReflectDirection = reflect(-DirectionToLight, Fragment.Normal);
                float SpecularValue = pow(max(dot(ViewDirection, ReflectDirection), 0.0), Material.ShineExponent);
                vec4 Specular = Fragment.Albedo.a * LightColor * SpecularValue * DiffuseValue;

                FinalColor += (Diffuse + Specular) * attenuation(Light, distance(Light.Position, Fragment.Position));
            }
            OutColor = FinalColor;
            break;
        }
        case MODE_ALBEDO: {
            OutColor = Fragment.Albedo;
            break;
        }
        case MODE_POSITION: {
            OutColor = vec4(Fragment.Position / 16, 1);
            OutColor.y *= -1;
            break;
        }
        case MODE_NORMAL: {
            OutColor = vec4(Fragment.Normal, 1);
            OutColor.y *= -1;
            break;
        }
        default: {
            OutColor = vec4(0.51, 0.96, 0.17, 1);
        }
    }
}
