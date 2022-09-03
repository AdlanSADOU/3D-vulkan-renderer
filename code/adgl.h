#pragma once
// note: This file must be included first
// as we are fleshing out the features, all includes that
// are generally shared across those features will go here.
// and this file is the first that will be included
//
// example:
// #include "adgl.h"
// #include "Mesh.h"
// #include "Animation.h"
//   etc......
//
// I find that this approach eases development and as features
// become stable, only then will they be moved into their own .h/.cpp pairs
// and include all their dependencies to be self sufficient.
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GLM/glm.hpp>
#include <stb_image.h>
#include <cgltf.h>

#include <vector>
#include <unordered_map>


struct Texture;
struct Material;

static std::unordered_map<std::string, Texture *>  gTextures;
static std::unordered_map<std::string, Material *> gMaterials;


#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

static void printVec(const char *name, glm::vec3 v)
{
    SDL_Log("%s: (%f, %f, %f)\n", name, v.x, v.y, v.z);
}

struct Transform
{
    glm::vec3 scale       = {};
    glm::vec3 rotation    = {};
    glm::vec3 translation = {};
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
