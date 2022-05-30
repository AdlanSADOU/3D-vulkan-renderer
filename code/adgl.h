#if !defined(ADGL_H)
#define ADGL_H

#define MATH_3D_IMPLEMENTATION
// #define GLM_FORCE_RADIANS 1
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL2/SDL.h>

#define u32 uint32_t

static void GLAPIENTRY MessageCallback(GLenum source,
    GLenum                                    type,
    GLuint                                    id,
    GLenum                                    severity,
    GLsizei                                   length,
    const GLchar                             *message,
    const void                               *userParam)
{
    SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
        type, severity, message);
}

#define ARR_COUNT(arr)(sizeof(arr)/sizeof(arr[0]))

struct Input
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool Q;
    bool E;
};

struct Vertex2D
{
    glm::vec2 vPos;
    glm::vec4 vColor;
    glm::vec2 vTexCoord;
};

struct Quad
{
    static inline Vertex2D vertices[4] = {
        { { +0.5f, +0.5f,}, { 1.0, 1.0, 1.0, 1.0 }, { 1, 1 } },
        { { +0.5f, -0.5f,}, { 1.0, 1.0, 1.0, 1.0 }, { 1, 0 } },
        { { -0.5f, -0.5f,}, { 1.0, 1.0, 1.0, 1.0 }, { 0, 0 } },
        { { -0.5f, +0.5f,}, { 1.0, 1.0, 1.0, 1.0 }, { 0, 1 } },
    };

    static inline GLuint indices[6] = {
        0,1,3,
        1,2,3
    };
};



void ExampleSPriteInit();
void ExampleSpriteUpdateDraw(float dt);

void Example3DInit();
void Example3DUpdateDraw(float dt, Input input);

#endif // ADGL_H
