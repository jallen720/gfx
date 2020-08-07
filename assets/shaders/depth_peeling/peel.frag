#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0, input_attachment_index = 0) uniform subpassInput PeelDepth;

layout(location = 0) in vec4 InColor;
layout(location = 0) out vec4 ColorAttachment;

void main() {
    if(gl_FragCoord.z <= subpassLoad(PeelDepth).r) discard;
    ColorAttachment = InColor;
}
