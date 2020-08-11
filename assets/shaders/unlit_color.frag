#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 OutColor;

layout(push_constant) uniform push_constants {
    vec4 Color;
} PushConstants;

void main() {
    OutColor = PushConstants.Color;
}
