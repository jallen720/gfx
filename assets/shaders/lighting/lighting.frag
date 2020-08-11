#version 450
#extension GL_ARB_separate_shader_objects : enable

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

layout(location = 0) out vec4 OutColor;

void main() {
    const vec4 AMBIENT = vec4(vec3(0.1), 1);
    vec4 Albedo = subpassLoad(AlbedoInput);
    vec4 Position = subpassLoad(PositionInput);
    vec4 Normal = subpassLoad(NormalInput);
    if(Position.x == 0 && Position.y == 0 && Position.z == 0) discard;
    vec4 FinalColor = Albedo * AMBIENT;
    for(uint LightIndex = 0; LightIndex < 2; ++LightIndex) {
        if(distance(Lights.Data[LightIndex].Position, vec3(Position)) < Lights.Data[LightIndex].Range) {
            FinalColor += Lights.Data[LightIndex].Color * 0.4;
        }
    }
    OutColor = FinalColor;
}
