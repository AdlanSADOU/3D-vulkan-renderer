#version 460
#extension GL_EXT_debug_printf : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 v_out_NORMAL;
layout (location = 1) in vec4 v_out_color;
layout (location = 2) in vec2 v_out_texcoord_0;
layout (location = 3) in flat uint v_out_base_color_texture_idx;

layout (location = 0) out vec4 Frag_Color;

layout(set = 3, binding = 0) uniform sampler   samp;
layout(set = 3, binding = 1) uniform texture2D textures[];

void main()
{
    float shade = dot(v_out_NORMAL, vec3(.5, .5, .4));

    Frag_Color = texture(sampler2D(textures[v_out_base_color_texture_idx], samp), v_out_texcoord_0) *  v_out_color * pow(shade,2)*1.7;
}