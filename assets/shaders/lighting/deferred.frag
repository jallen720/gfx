#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D AlbedoTexture;

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUV;
layout(location = 0) out vec4 OutAlbedo;
layout(location = 1) out vec4 OutPosition;
layout(location = 2) out vec4 OutNormal;

void main() {
    OutAlbedo = texture(AlbedoTexture, InUV);
    OutNormal = vec4(InNormal, 1);
    OutPosition = vec4(InPosition, 1);
}
