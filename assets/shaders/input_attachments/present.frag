#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInput InputDepth;
layout(set = 0, binding = 1, input_attachment_index = 1) uniform subpassInput InputColor;

layout(location = 0) out vec4 OutColor;

void
main()
{
    if(gl_FragCoord.x < 640)
    {
        OutColor.rgb = subpassLoad(InputColor).rgb;
    }
    else
    {
        OutColor.rgb = subpassLoad(InputDepth).rgb;
    }
}
