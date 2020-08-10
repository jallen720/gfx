#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0, std140) uniform entity {
    mat4 ModelMatrix;
    mat4 MVPMatrix;
} Entity;

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUV;
layout(location = 0) out vec3 OutPosition;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec2 OutUV;

void main() {
    gl_Position = Entity.MVPMatrix * vec4(InPosition, 1);
    OutPosition = InPosition;
    OutNormal = InNormal;
    OutUV = InUV;
}
