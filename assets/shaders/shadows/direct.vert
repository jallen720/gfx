#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 space_mtx;
    vec3 pos;
} light_ubo;

layout (set = 1, binding = 0, std140) uniform u_model_ubo {
    mat4 model_mtx;
    mat4 mvp_mtx;
} model_ubo;

layout (location = 0) in vec3 in_vert_pos;
layout (location = 1) in vec3 in_vert_normal;
layout (location = 2) in vec2 in_vert_uv;
layout (location = 0) out vec4 out_frag_pos_light_space;
layout (location = 1) out vec3 out_frag_normal;
layout (location = 2) out vec2 out_frag_uv;

// Converts fragment's x/y coordinates from NDC-space: [-1..1] to uv-space: [0..1] for sampling shadow map.
const mat4 ndc_to_uv_mtx = mat4(0.5f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.5f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.5f, 0.5f, 0.0f, 1.0f);

void main() {
    vec4 vert_pos = vec4(in_vert_pos, 1);
    gl_Position = model_ubo.mvp_mtx * vert_pos;
    out_frag_pos_light_space = ndc_to_uv_mtx * light_ubo.space_mtx * model_ubo.model_mtx * vert_pos;
    out_frag_normal = mat3(model_ubo.model_mtx) * in_vert_normal;
    out_frag_uv = in_vert_uv;
}
