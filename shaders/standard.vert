#version 450 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormals;
layout (location = 2) in vec2 vTex;

out vec2 texCoord;
out vec4 color;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    // vec4 point = projectionGLM * vec4(1.0f, 1.0f, 1.0f, 1.0f);
    gl_Position = projection * view * model * vec4(vPos, 1.0f);
    texCoord = vTex;
    color = vec4(1,1,1,1);
}