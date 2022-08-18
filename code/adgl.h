#if !defined(ADGL_H)
#define ADGL_H

#include "mymath.h"
#include "stb_image.h"

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GLM/glm.hpp>
#include <SDL2/SDL.h>
#include <vector>


#define u32 uint32_t
#define u16 uint16_t

struct Vertex
{
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;
};

struct VertexSOA
{
    Vector3 *position;
    Vector3 *normal;
    Vector2 *texCoord;
};

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

static void printVec(const char *name, Vector3 v)
{
    SDL_Log("%s: (%f, %f, %f)\n", name, v.x, v.y, v.z);
}

static float triangle_verts[] = {
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.0f, 0.5f, -0.5f
};

static std::vector<Vertex> cube_verts = {
    // front
    { -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0 },
    { 1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0 },
    { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
    { -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0 },
    // back
    { -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 0.0, 0.0 },
    { 1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 0.0 },
    { 1.0, 1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
    { -1.0, 1.0, -1.0, 1.0, 1.0, 1.0, 0.0, 1.0 }
};

static std::vector<u16> cube_indices = {
    // front
    0, 1, 2,
    2, 3, 0,
    // right
    1, 5, 6,
    6, 2, 1,
    // back
    7, 6, 5,
    5, 4, 7,
    // left
    4, 0, 3,
    3, 7, 4,
    // bottom
    4, 5, 1,
    1, 0, 4,
    // top
    3, 2, 6,
    6, 7, 3
};

static u16 triangle_indices[] = {
    0, 1, 2
};


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
