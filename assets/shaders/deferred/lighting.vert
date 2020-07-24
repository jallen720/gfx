#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 0) out vec2 OutUV;

void
main()
{
    gl_Position = vec4(InPosition, 1);
    OutUV = InUV;
}
