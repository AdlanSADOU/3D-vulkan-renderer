#version 450 core

layout (location = 0) in vec2 vPos;
layout (location = 1) in vec4 vColor;
layout (location = 2) in vec2 vTexCoord;

out vec4 inColor;
out vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float i;

void main()
{
    mat4 view_model = view * model;
    // projection *

   gl_Position = projection * view_model * vec4(vPos, 0.0, 1.0);
   inColor = vColor;
   texCoord = vTexCoord;
}
