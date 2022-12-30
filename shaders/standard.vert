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
    vec3 pos;
} camera;



struct ObjectData {
    mat4 model;
    mat4 joint_matrices[128];
};

layout(std430, set = 1, binding = 0) readonly buffer ObjectDataBuffer {
    ObjectData data[];
} object;


layout(std430, push_constant) uniform PushConstants {
    int object_data_idx;
    int has_joints;
    int is_skybox;
} constants;


//OUT
layout (location = 0) out int vsBaseInstance;
layout (location = 1) out int vsIsSkybox;
layout (location = 2) out vec3 vsNORMAL;
layout (location = 3) out vec2 vsTexcoord_0;
layout (location = 4) out vec3 vsSkyboxUVW;
layout (location = 5) out vec3 vsViewDir;

layout (location = 6) out vec3 vsViewSpaceVertPos;

void main()
{
    int obj_idx = constants.object_data_idx;
    mat4 model = object.data[obj_idx].model;

    mat4 skin_matrix = mat4(1);

    if (constants.has_joints == int(1) && constants.is_skybox == int(-1)){
        skin_matrix =
            WEIGHTS_0.x * object.data[obj_idx].joint_matrices[uint(JOINTS_0.x*255)] + // *255 because .xyz come in as normalized floats!
            WEIGHTS_0.y * object.data[obj_idx].joint_matrices[uint(JOINTS_0.y*255)] +
            WEIGHTS_0.z * object.data[obj_idx].joint_matrices[uint(JOINTS_0.z*255)] +
            WEIGHTS_0.w * object.data[obj_idx].joint_matrices[uint(JOINTS_0.w*255)];
    }

    // gl_Position = camera.projection * camera.view * vec4(POSITION, 1);


    mat4 actual_view;

    if (constants.is_skybox == int(1))
    {
        vsSkyboxUVW = POSITION;
        // remove translation when rendering the skybox
        // so that our camera is always centered in it
        actual_view = mat4(mat3(camera.view));
    }
    else
    {
        // vsNORMAL =  (transpose(inverse(mat3(model * skin_matrix))) * NORMAL);
        vsNORMAL =  normalize(mat3(model * skin_matrix) * NORMAL);
        vsTexcoord_0 = TEXCOORD_0;
        actual_view = camera.view;
    }

    vsBaseInstance = gl_BaseInstance;
    vsIsSkybox = constants.is_skybox;
    vsViewDir = normalize(vec4(model * skin_matrix) * vec4(POSITION, 1) - vec4(camera.pos, 1)).xyz * -1;

    vec4 vertPos4 = actual_view * model * skin_matrix * vec4(POSITION, 1);
    vsViewSpaceVertPos = vertPos4.xyz / vertPos4.w; // why?

    gl_Position = camera.projection * actual_view * model * skin_matrix * vec4(POSITION, 1);
}