#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0, std140) uniform entity
{
    mat4 ModelMatrix;
    mat4 MVPMatrix;
}
Entity;

layout(location = 0) in vec3 InPosition;
layout(location = 0) out vec4 OutColor;

void
main()
{
    gl_Position = Entity.MVPMatrix * vec4(InPosition, 1);
    OutColor = vec4(InPosition.r, -InPosition.g, InPosition.b, 0.5);
}
