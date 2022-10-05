#version 450
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 COLOR;

//VS_OUT
layout (location = 0) out vec4 v_out_color;

void main()
{
    gl_Position = vec4(POSITION, 1);

    v_out_color = vec4(COLOR, 1);

    // debugPrintfEXT("POS(%f, %f, %f) COL(%f, %f, %f)\n", POSITION.x, POSITION.y, POSITION.z, COLOR.x, COLOR.y, COLOR.z);
}