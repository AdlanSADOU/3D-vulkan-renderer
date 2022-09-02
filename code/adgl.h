#if !defined(ADGL_H)
#define ADGL_H


#include "stb_image.h"
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GLM/glm.hpp>
#include <SDL2/SDL.h>
#include <vector>
#include <unordered_map>


struct Texture;
struct Material;

static std::unordered_map<std::string, Texture *>  gTextures;
static std::unordered_map<std::string, Material *> gMaterials;

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec4 joints;
    glm::vec4 weights;
};

struct VertexSOA
{
    glm::vec3 *position;
    glm::vec3 *normal;
    glm::vec2 *texCoord;
    glm::vec4 *joints;
    glm::vec4 *weights;
};

struct Transform
{
    glm::vec3 scale       = {};
    glm::vec3 rotation    = {};
    glm::vec3 translation = {};
};

static void GLAPIENTRY MessageCallback(GLenum source,
    GLenum                                    type,
    GLuint                                    id,
    GLenum                                    severity,
    GLsizei                                   length,
    const GLchar                             *message,
    const void                               *userParam)
{
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
        SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
}

#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

static void printVec(const char *name, glm::vec3 v)
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
    { { -1.0, -1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 0.0 } },
    { { 1.0, -1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 1.0, 0.0 } },
    { { 1.0, 1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 1.0, 1.0 } },
    { { -1.0, 1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0 } },
    // back
    { { -1.0, -1.0, -1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 0.0 } },
    { { 1.0, -1.0, -1.0 }, { 1.0, 1.0, 1.0 }, { 1.0, 0.0 } },
    { { 1.0, 1.0, -1.0 }, { 1.0, 1.0, 1.0 }, { 1.0, 1.0 } },
    { { -1.0, 1.0, -1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0 } }
};

static std::vector<uint16_t> cube_indices = {
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

static uint16_t triangle_indices[] = {
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
