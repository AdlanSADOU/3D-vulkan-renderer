#version 460 // for gl_BaseInstance
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 NORMAL;
layout (location = 2) in vec2 TEXCOORD_0;
layout (location = 3) in vec2 TEXCOORD_1;
layout (location = 4) in vec4 JOINTS_0;
layout (location = 5) in vec4 WEIGHTS_0;


layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 projection;
    mat4 view;
} camera;



struct ObjectData {
    mat4 model;
    mat4 joint_matrices[128];
};

layout(std430, set = 1, binding = 0) readonly buffer ObjectDataBuffer {
    ObjectData data[];
} object;


layout(push_constant) uniform PushConstants {
    int object_data_idx;
    int has_joints;
    int pad[2];
} constants;


//OUT
layout (location = 0) out vec3 v_out_NORMAL;
layout (location = 1) out vec2 v_out_texcoord_0;
layout (location = 2) out int baseInstance;

void main()
{
    int obj_idx = constants.object_data_idx;
    mat4 skin_matrix = mat4(1);
    debugPrintfEXT("[%d] i[%d]\n", obj_idx, baseInstance);
    // debugPrintfEXT("[%f, %f, %f, %f] :: [%d, %d, %d, %d]",
    //     WEIGHTS_0.x,WEIGHTS_0.y,WEIGHTS_0.z,WEIGHTS_0.w ,
    //     JOINTS_0.x,JOINTS_0.y,JOINTS_0.z,JOINTS_0.w);

    if (constants.has_joints == int(1)){
        skin_matrix =
            WEIGHTS_0.x * object.data[obj_idx].joint_matrices[uint(JOINTS_0.x*255)] +
            WEIGHTS_0.y * object.data[obj_idx].joint_matrices[uint(JOINTS_0.y*255)] +
            WEIGHTS_0.z * object.data[obj_idx].joint_matrices[uint(JOINTS_0.z*255)] +
            WEIGHTS_0.w * object.data[obj_idx].joint_matrices[uint(JOINTS_0.w*255)];
    }

    gl_Position = camera.projection * camera.view * object.data[obj_idx].model * skin_matrix * vec4(POSITION, 1);

    // v_out_NORMAL =  NORMAL;
    v_out_NORMAL =  normalize(transpose(inverse(mat3(object.data[obj_idx].model * skin_matrix))) * NORMAL);;
    v_out_texcoord_0 = TEXCOORD_0;
    baseInstance = gl_BaseInstance;

}