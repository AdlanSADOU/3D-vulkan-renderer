#version 450

uniform mat4 proj;
uniform mat4 view;

out vec2 texCoord;

vec3 verts[] = {
    vec3(-1, 0, -1), vec3(1, 0, -1),
    vec3(-1, 0, 1), vec3(1, 0, 1)
};

int indices[] = {
    0, 1, 2,
    1, 3, 2
};

vec2 uv[] = {
    vec2(0, 1), vec2(1, 1),
    vec2(0, 0), vec2(1, 1),
    vec2(1, 0), vec2(0, 0)
};

void main()
{
    gl_Position = proj * view * vec4(verts[indices[gl_VertexID]].xyz * 100, 1);
    texCoord = uv[gl_VertexID];
}