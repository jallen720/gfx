#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D AlbedoMap;
layout(set = 0, binding = 1) uniform sampler2D PositionMap;

layout(location = 0) in vec2 InUV;
layout(location = 0) out vec4 OutColor;

void
main()
{
    // OutColor = texture(AlbedoMap, InUV);
    OutColor = texture(PositionMap, InUV);
}
