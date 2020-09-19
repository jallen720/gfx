#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform sampler2D tex;

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

void main() {
    out_color = vec4(vec3(texture(tex, in_uv).r), 1);
}
