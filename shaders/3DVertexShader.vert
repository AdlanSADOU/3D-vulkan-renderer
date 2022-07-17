#version 330 core

layout (location = 0) in vec3 vPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 transform;

void main()
{
    // vec4 point = projectionGLM * vec4(1.0f, 1.0f, 1.0f, 1.0f);
    gl_Position = projection * view * transform * vec4(vPos, 1.0f);
}