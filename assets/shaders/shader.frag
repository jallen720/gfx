#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D ColorTexture;

layout(location = 0) in vec2 InUV;
layout(location = 0) out vec4 OutColor;

void
main()
{
    OutColor = texture(ColorTexture, InUV);
}
