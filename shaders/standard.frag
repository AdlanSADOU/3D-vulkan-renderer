#version 450
#extension GL_EXT_debug_printf : enable
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec4 v_out_color;

layout (location = 0) out vec4 Frag_Color;

void main()
{
    Frag_Color = v_out_color;
    // gl_FragDepth = .5;
    // debugPrintfEXT("w:%f\n", gl_FragDepth);

}