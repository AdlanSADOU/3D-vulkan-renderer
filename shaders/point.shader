#VERT_START
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
#VERT_END


// this area is not parsed
// do whatever you want with it


#FRAG_START

#version 450 core

in vec4 color;

out vec4 FragColor;


void main()
{
    // FragColor = mix(texture(texSampler, texCoord), texture(texSampler1, texCoord), 0.2) * inColor;
    // FragColor = texture(texSampler, texCoord) * inColor;
    FragColor = color;
}

#FRAG_END