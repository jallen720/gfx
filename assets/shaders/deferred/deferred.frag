#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D AlbedoMap;

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec4 InWorldPosition;
layout(location = 0) out vec4 OutAlbedo;
layout(location = 1) out vec4 OutPosition;


void
main()
{
    OutAlbedo = texture(AlbedoMap, InUV);
    OutPosition = InWorldPosition;
}
