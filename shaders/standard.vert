#version 450
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 NORMAL;

layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} constants;

//VS_OUT
layout (location = 0) out vec3 v_out_NORMAL;

void main()
{
    gl_Position = ubo.projection * ubo.view * constants.model * vec4(POSITION, 1);

    // v_out_NORMAL =  vec4(ubo.projection*vec4(NORMAL,1)).xyz;
    v_out_NORMAL =  NORMAL;
}