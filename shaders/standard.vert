#version 460
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require

// precision highp int;

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 NORMAL;
layout (location = 2) in vec2 TEXCOORD_0;
layout (location = 3) in vec2 TEXCOORD_1;
layout (location = 4) in vec4 WEIGHTS_0;
layout (location = 5) in vec4 JOINTS_0;


layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 projection;
    mat4 view;
} camera;


layout(push_constant) uniform PushConstants {
    uint draw_data_idx;
} constants;


struct ObjectData {
    mat4 model;
    mat4 joint_matrices[64];
};

layout(set = 1, binding = 0) buffer ObjectDataBuffer {
    ObjectData data[];
} object;



//OUT
layout (location = 0) out vec3 v_out_NORMAL;
layout (location = 1) out vec2 v_out_texcoord_0;
layout (location = 2) out int baseInstance;

void main()
{
    gl_Position = camera.projection * camera.view * object.data[constants.draw_data_idx].model * vec4(POSITION, 1);

    v_out_NORMAL =  NORMAL;
    v_out_texcoord_0 = TEXCOORD_0;
    baseInstance = gl_BaseInstance;

}