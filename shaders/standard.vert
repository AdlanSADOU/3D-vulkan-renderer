#version 450
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 NORMALS;

layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    mat4 model;
} ubo;

//VS_OUT
layout (location = 0) out vec4 v_out_color;

void main()
{
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(POSITION, 1);

    v_out_color = vec4(NORMALS, 1);

    // debugPrintfEXT("POS(%f, %f, %f) COL(%f, %f, %f)\n", POSITION.x, POSITION.y, POSITION.z, COLOR.x, COLOR.y, COLOR.z);
}