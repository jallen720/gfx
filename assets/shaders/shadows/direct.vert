#version 450
#extension GL_ARB_separate_shader_objects : enable

#define LIGHT_MODE_DIRECTIONAL 0
#define LIGHT_MODE_POINT 1

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    vec4 color;
    vec3 pos;
    int mode;
    vec3 dir;
    float far_clip;
    int depth_bias;
    int normal_bias;
    float linear;
    float quadratic;
    float ambient;
} light_ubo;
layout (set = 1, binding = 0, std140) uniform u_model_ubo {
    mat4 model_mtx;
    mat4 mvp_mtx;
} model_ubo;

layout (location = 0) in vec3 in_vert_pos;
layout (location = 1) in vec3 in_vert_norm;
layout (location = 2) in vec2 in_vert_uv;

layout (location = 0) out vec3 out_frag_pos;
layout (location = 1) out vec4 out_frag_pos_light_space;
layout (location = 2) out vec3 out_frag_norm;
layout (location = 3) out vec2 out_frag_uv;
layout (location = 4) out vec3 out_frag_light_dir;

// Converts fragment's x/y coordinates from NDC-space: [-1..1] to uv-space: [0..1] for sampling shadow map.
const mat4 ndc_to_uv_mtx = mat4(0.5, 0.0, 0.0, 0.0,
                                0.0, 0.5, 0.0, 0.0,
                                0.0, 0.0, 1.0, 0.0,
                                0.5, 0.5, 0.0, 1.0);

void main() {
    vec4 vert_pos = vec4(in_vert_pos, 1);
    gl_Position = model_ubo.mvp_mtx * vert_pos;
    out_frag_pos = vec3(model_ubo.model_mtx * vert_pos);
    out_frag_norm = transpose(inverse(mat3(model_ubo.model_mtx))) * in_vert_norm;
    vec3 frag_norm_bias = out_frag_norm * light_ubo.normal_bias * 0.001;
    out_frag_pos_light_space = /*ndc_to_uv_mtx * light_ubo.view_mtxs[0] * */ vec4(out_frag_pos + frag_norm_bias, 1);
    out_frag_uv = in_vert_uv;
    out_frag_light_dir = light_ubo.mode == LIGHT_MODE_DIRECTIONAL
                         ? -light_ubo.dir
                         : light_ubo.pos - vec3(model_ubo.model_mtx * vert_pos);
}
