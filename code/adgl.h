#if !defined(ADGL_H)
#define ADGL_H


#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL2/SDL.h>

#include "stb_image.h"

#define u32 uint32_t
#define u16 uint16_t

static void GLAPIENTRY MessageCallback(GLenum source,
    GLenum                                    type,
    GLuint                                    id,
    GLenum                                    severity,
    GLsizei                                   length,
    const GLchar *                            message,
    const void *                              userParam)
{
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
        type, severity, message);
}

#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

struct Input
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool Q;
    bool E;

    struct Mouse
    {
        int32_t xrel;
        int32_t yrel;
        bool    left;
        bool    right;
    } mouse;
};



void ExampleSPriteInit();
void ExampleSpriteUpdateDraw(float dt, Input input);

void Example3DInit();
void Example3DUpdateDraw(float dt, Input input);

void InitSpaceShooter();
void UpdateDrawSpaceShooter(float dt, Input input);

#endif // ADGL_H
