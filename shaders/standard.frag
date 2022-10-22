#version 460
#extension GL_EXT_debug_printf : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 v_out_NORMAL;
layout (location = 1) in vec2 v_out_texcoord_0;
layout (location = 2) in flat int baseInstance;

layout (location = 0) out vec4 Frag_Color;

layout(set = 3, binding = 0) uniform sampler   samp;
layout(set = 3, binding = 1) uniform texture2D textures[];

// per mesh data
struct MaterialData
{
    int base_color_texture_idx;
    int emissive_texture_idx;
    int normal_texture_idx;
    int metallic_roughness_texture_idx;
    float tiling_x;
    float tiling_y;
    float metallic_factor;
    float roughness_factor;
    vec4 base_color_factor;
};
layout(set = 2, binding = 0)  buffer SSBO {
    MaterialData data[];
} material;

void main()
{
    float shade = dot(v_out_NORMAL, vec3(.5, .5, .4));

    // v_out_color = material.data[gl_BaseInstance].color;
    MaterialData mat = material.data[baseInstance];

    int v_out_base_color_texture_idx = mat.base_color_texture_idx;

    if (mat.base_color_texture_idx > -1)
    {
        Frag_Color = texture(sampler2D(textures[v_out_base_color_texture_idx], samp), vec2(v_out_texcoord_0.x * mat.tiling_x, v_out_texcoord_0.y * mat.tiling_y)) * shade * mat.base_color_factor;
    } else {
        Frag_Color = mat.base_color_factor * shade;

    }
    // debugPrintfEXT("v_out_base_color_texture_idx[%d]\n", v_out_base_color_texture_idx);
}