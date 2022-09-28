#VERT_START
#version 450 core

layout (location = 0) in vec3 POSITION;
layout (location = 1) in vec3 NORMAL;
layout (location = 2) in vec2 TEXCOORD_0;
layout (location = 3) in vec2 TEXCOORD_1;
layout (location = 4) in vec4 JOINTS_0;
layout (location = 5) in vec4 WEIGHTS_0;

out vec2 texCoord_0;
out vec2 texCoord_1;
out vec4 color;
out vec3 normal;
out vec3 fragPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform mat4 joint_matrices[128];// might want to switch to SSBOs
uniform int has_joint_matrices;


void main()
{
    // vec4 point = projectionGLM * vec4(1.0f, 1.0f, 1.0f, 1.0f);
    mat4 modelViewProj = projection * view * model;
    mat4 skinMatrix = mat4(1);

    if (has_joint_matrices == 1)
    {
        skinMatrix =
        WEIGHTS_0.x * joint_matrices[uint(JOINTS_0.x)]+
        WEIGHTS_0.y * joint_matrices[uint(JOINTS_0.y)]+
        WEIGHTS_0.z * joint_matrices[uint(JOINTS_0.z)];
        (1-WEIGHTS_0.x-WEIGHTS_0.y-WEIGHTS_0.z) * joint_matrices[uint(JOINTS_0.w)];
    }

    gl_Position = modelViewProj * skinMatrix * vec4(POSITION, 1.0f);
    fragPos = (model * skinMatrix * vec4(POSITION, 1.0f)).xyz;


    texCoord_0 = TEXCOORD_0;
    // texCoord_1 = TEXCOORD_1;
    normal =  NORMAL;
    color = vec4(1,1,1,1);

}

#VERT_END
#FRAG_START

#version 450 core

in vec2 texCoord_0;
in vec2 texCoord_1;
in vec4 color;
in vec3 normal;
in vec3 fragPos;
out vec4 FragColor;


uniform vec3 view_pos;
uniform vec3 light_dir;

uniform sampler2D mapBaseColor;

void main()
{
    // FragColor = mix(texture(mapBaseColor, texCoord_0), texture(texSampler1, texCoord_0), 0.2) * inColor;
    vec4 radiance = vec4(color.xyz * dot(normal, -light_dir) + vec3(.9, .9, .9) *1., 1.);
    // FragColor =  radiance;
    FragColor = texture(mapBaseColor, texCoord_0) * radiance;
    // FragColor = vec4(normal, 1.);
}

#FRAG_END