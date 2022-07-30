#version 330 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec2 vTex;

out vec2 texCoord;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    // vec4 point = projectionGLM * vec4(1.0f, 1.0f, 1.0f, 1.0f);
    gl_Position = projection * view * model * vec4(vPos, 1.0f);
    texCoord = vTex;
}