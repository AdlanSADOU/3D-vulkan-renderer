#version 450
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 NORMALS;

layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} constants;

//VS_OUT
layout (location = 0) out vec4 v_out_color;

void main()
{
    gl_Position = ubo.projection * ubo.view * constants.model * vec4(POSITION, 1);

    v_out_color = vec4(NORMALS, 1);
}