#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput PeelColor;

layout(location = 0) out vec4 ColorAttachment;

void main() {
   vec4 Color = subpassLoad(PeelColor);
   ColorAttachment = vec4(vec3(Color.rgb * Color.a), Color.a);
}
