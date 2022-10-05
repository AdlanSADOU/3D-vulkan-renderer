#VERT_START
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
    gl_Position = proj * view * vec4(verts[indices[gl_VertexID]].xyz * 1000, 1);
    texCoord = uv[gl_VertexID];
}
#VERT_END

#FRAG_START
#version 450 core

// in vec4 inColor;
in vec2 texCoord;

out vec4 FragColor;

uniform sampler2D texSampler;

void main()
{
    FragColor = texture(texSampler, texCoord*60) * vec4(1,1,1, gl_FragCoord.w*400/*distance fog effect hack*/);
    // FragColor = vec4(1,0,0,1);
}

#FRAG_END