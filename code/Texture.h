#pragma once

struct Texture
{
    GLuint id;
    int    width;
    int    height;
    int    channelCount;
};

static Texture *TextureCreate(const char *path)
{
    // stbi_set_flip_vertically_on_load(true);


    Texture *t = (Texture *)malloc(sizeof(Texture));
    assert(t);

    unsigned char *pixels = stbi_load(path, &t->width, &t->height, &t->channelCount, 0);
    if (!pixels) {
        SDL_LogCritical(0, "Failed to load texture: %s", path);
        free(t);
        return NULL;
    }

    // note: if we comment these out, texture still repeats
    // but in a different way...?
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    if (t->channelCount == 4)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->width, t->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    else if (t->channelCount == 3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, t->width, t->height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(pixels);

    return t;
}