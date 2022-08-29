#version 450 core

layout (location = 0) in vec3 vPos;

out vec4 color;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(vPos, 1.0f) *.1f;
    color = vec4(1.f,.8f,0.2f,.1f);
}