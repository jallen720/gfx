#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0, std140) uniform entity
{
    mat4 ModelMatrix;
    mat4 MVPMatrix;
}
Entity;

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUV;
layout(location = 0) out vec2 OutUV;
layout(location = 1) out vec4 OutWorldPosition;

void
main()
{
    gl_Position = Entity.MVPMatrix * vec4(InPosition, 1);
    OutWorldPosition = Entity.ModelMatrix * vec4(InPosition, 1);
    OutWorldPosition.y *= -1;
    OutUV = InUV;
}
