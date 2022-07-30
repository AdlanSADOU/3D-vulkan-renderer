#version 450 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec4 color;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

mat4 viewproj;

out vec4 outColor;

void main()
{
    viewproj = projection * view;
    gl_Position = viewproj * model * vec4(pos, 1.f);
    outColor = color;
}