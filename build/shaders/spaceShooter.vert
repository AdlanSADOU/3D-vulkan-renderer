#version 450 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec4 color;

uniform mat4 viewproj;
uniform mat4 model;

out vec4 outColor;

void main()
{
    gl_Position = vec4(pos, 1.f);
    outColor = color;
}