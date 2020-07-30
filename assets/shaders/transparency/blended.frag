#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D AlbedoTexture;

layout(location = 0) in vec2 InUV;
layout(location = 0) out vec4 OutColor;

void
main()
{
    vec4 SampledAlbedo = texture(AlbedoTexture, InUV);
    // if (SampledAlbedo.a < 1) discard;
    OutColor = texture(AlbedoTexture, InUV);
}
