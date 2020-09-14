#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 1, binding = 0) uniform sampler2D albedo_map;

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

void main() {
    out_color = texture(albedo_map, in_uv);
}
