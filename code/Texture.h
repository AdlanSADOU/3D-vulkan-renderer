#pragma once

#include "adgl.h"

struct Texture
{
    GLuint id;
    int    tex_width, tex_height, nrChannels;

    void Create(const char *path)
    {
        stbi_set_flip_vertically_on_load(true);


        unsigned char *pixels = stbi_load(path, &tex_width, &tex_height, &nrChannels, 0);
        if (!pixels) {
            SDL_LogCritical(0, "Failed to load texture: %s", path);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        if (nrChannels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        else if (nrChannels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(pixels);
    }
};
