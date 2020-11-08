#version 450
#extension GL_ARB_separate_shader_objects : enable

#define LIGHT_MODE_DIRECTIONAL 0
#define LIGHT_MODE_POINT 1

layout (set = 0, binding = 0, std140) uniform u_light_ubo {
    mat4 view_mtxs[6];
    vec3 pos;
    vec3 direction;
    int mode;
    vec4 color;
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
layout (push_constant) uniform u_push_constants {
    uint direction_view_mtx_idx;
} push_constants;
layout (location = 0) in vec3 in_vert_pos;
layout (location = 0) out vec3 out_frag_pos;

void main() {
    uint view_mtx_idx = light_ubo.mode == LIGHT_MODE_DIRECTIONAL ? 0 : push_constants.direction_view_mtx_idx;
    out_frag_pos = vec3(model_ubo.model_mtx * vec4(in_vert_pos, 1));
    gl_Position = light_ubo.view_mtxs[view_mtx_idx] * model_ubo.model_mtx * vec4(in_vert_pos, 1);
}
