#version 450 core

// in vec4 inColor;
in vec2 texCoord;

out vec4 FragColor;

uniform sampler2D texSampler;

void main()
{
    FragColor = texture(texSampler, texCoord * 20);
    // FragColor = vec4(1,0,0,1);
}
