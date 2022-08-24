#version 450 core

in vec4 color;

out vec4 FragColor;


void main()
{
    // FragColor = mix(texture(texSampler, texCoord), texture(texSampler1, texCoord), 0.2) * inColor;
    // FragColor = texture(texSampler, texCoord) * inColor;
    FragColor = color;
}
