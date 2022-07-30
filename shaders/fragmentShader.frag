#version 450 core

// in vec4 inColor;
in vec2 texCoord;

out vec4 FragColor;

uniform sampler2D texSampler;

void main()
{
    // FragColor = mix(texture(texSampler, texCoord), texture(texSampler1, texCoord), 0.2) * inColor;
    // FragColor = texture(texSampler, texCoord) * inColor;
    FragColor = texture(texSampler, texCoord);
}
