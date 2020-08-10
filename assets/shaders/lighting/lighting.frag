#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput AlbedoInput;
layout(set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput PositionInput;
layout(set = 0, binding = 2, input_attachment_index = 2) uniform subpassInput NormalInput;

layout(location = 0) out vec4 OutColor;

void main() {
    if(gl_FragCoord.x > 640) {
        OutColor = subpassLoad(AlbedoInput);
    } else {
        if(gl_FragCoord.y < 360) {
            OutColor = subpassLoad(PositionInput);
        } else {
            OutColor = subpassLoad(NormalInput);
        }
    }
}
