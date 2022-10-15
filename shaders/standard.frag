#version 450
#extension GL_EXT_debug_printf : enable
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 v_out_NORMAL;

layout (location = 0) out vec4 Frag_Color;

void main()
{
    float shade = dot(v_out_NORMAL, vec3(.5, .5, .4));
    Frag_Color = vec4(1, 1,1,1) * pow(shade,2)*1.7;

}