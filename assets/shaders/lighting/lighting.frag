#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MODE_COMPOSITE 0
#define MODE_ALBEDO 1
#define MODE_POSITION 2
#define MODE_NORMAL 3

struct light {
    vec3 Position;
    float Range;
    vec4 Color;
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

void main() {
    vec4 Albedo = subpassLoad(AlbedoInput);
    vec4 Position = subpassLoad(PositionInput);
    vec4 Normal = subpassLoad(NormalInput);

    switch(PushConstants.Mode) {
        case MODE_COMPOSITE: {
            const vec4 AMBIENT = vec4(vec3(0.1), 1);
            if(Position.x == 0 && Position.y == 0 && Position.z == 0) discard;
            vec4 FinalColor = Albedo * AMBIENT;
            for(uint LightIndex = 0; LightIndex < 2; ++LightIndex) {
                if(distance(Lights.Data[LightIndex].Position, vec3(Position)) < Lights.Data[LightIndex].Range) {
                    FinalColor += Lights.Data[LightIndex].Color * 0.4;
                }
            }
            OutColor = FinalColor;
            break;
        }
        case MODE_ALBEDO: {
            OutColor = Albedo;
            break;
        }
        case MODE_POSITION: {
            OutColor = Position / 16;
            OutColor.y *= -1;
            break;
        }
        case MODE_NORMAL: {
            OutColor = Normal;
            OutColor.y *= -1;
            break;
        }
        default: {
            OutColor = vec4(0.51, 0.96, 0.17, 1);
        }
    }
}
