#version 460
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require

precision highp int;

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 NORMAL;
layout (location = 2) in vec2 TEXCOORD_0;


layout(set = 0, binding = 0) uniform ViewProjData {
    mat4 projection;
    mat4 view;
} ubo;

// Per object data
struct ObjectData {
    mat4 model;
};
layout(set = 1, binding = 0) buffer ObjectDataBuffer {
    ObjectData data[];
} object;

layout(push_constant) uniform PushConstants {
    uint draw_data_idx;
} constants;

// per mesh data
struct MaterialData
{
    vec4 color;
    uint base_color_texture_idx;
    uint emissive_texture_idx;
    uint normal_texture_idx;
};
layout(set = 2, binding = 0)  buffer SSBO {
    MaterialData data[];
} material;


//OUT
layout (location = 0) out vec3 v_out_NORMAL;
layout (location = 1) out vec4 v_out_color;
layout (location = 2) out vec2 v_out_texcoord_0;
layout (location = 3) out uint v_out_base_color_texture_idx;

void main()
{
    gl_Position = ubo.projection * ubo.view * object.data[constants.draw_data_idx].model * vec4(POSITION, 1);

    // gl_Position = ubo.projection * ubo.view * vec4(POSITION, 1);
    // v_out_NORMAL =  vec4(ubo.projection*vec4(NORMAL,1)).xyz;
    v_out_NORMAL =  NORMAL;
    v_out_texcoord_0 = TEXCOORD_0;
    v_out_color = material.data[gl_BaseInstance].color;
}