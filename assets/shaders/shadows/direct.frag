#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 2, binding = 0) uniform sampler2D tex;
// layout (set = 2, binding = 1) uniform sampler2D shadow_map;

layout (location = 0) in vec2 in_uv;
layout (location = 1) in vec4 in_light_space_frag_pos;
layout (location = 0) out vec4 out_color;

void main() {
    out_color = texture(tex, in_uv);
}
