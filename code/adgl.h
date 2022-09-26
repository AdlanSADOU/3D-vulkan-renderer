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
#include <GLM/gtx/quaternion.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <cgltf.h>

#include <vector>
#include <unordered_map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "utils.h"

struct Texture;
struct Material;
struct MeshData;
struct Shader;

static std::unordered_map<std::string, Texture *>  gTextures;
static std::unordered_map<std::string, Shader *>   gShaders;
static std::unordered_map<std::string, Material *> gMaterials;
static std::unordered_map<std::string, void *>     gSharedMeshes;

#define NB_OF_ELEMENTS_IN_ARRAY(arr) (sizeof(arr) / sizeof(arr[0]))

static void printVec(const char *name, glm::vec3 v)
{
    SDL_Log("%s: (%f, %f, %f)\n", name, v.x, v.y, v.z);
}

struct Transform
{
    const char *name        = {};
    Transform  *parent      = {};
    Transform  *child       = {};
    glm::vec3   scale       = {};
    glm::quat   rotation    = {};
    glm::vec3   translation = {};
    glm::mat4   GetLocalMatrix();
    glm::mat4   ComputeGlobalMatrix();
};

glm::mat4 Transform::GetLocalMatrix()
{

    return glm::translate(glm::mat4(1), translation)
        * glm::toMat4(rotation)
        * glm::scale(glm::mat4(1), scale);
}

glm::mat4 Transform::ComputeGlobalMatrix()
{
    if (!parent) return GetLocalMatrix();

    glm::mat4  globalMatrix = glm::mat4(1);
    Transform *tmp          = this;

    // ummm... yeah, this will be changed eventually
    // it's the first iterative solution I came up with..
    std::list<Transform *> hierarchy;
    while (tmp->parent) {
        tmp = tmp->parent;
        hierarchy.push_front(tmp);
    }

    for (auto &&i : hierarchy) {
        globalMatrix *= i->GetLocalMatrix();
    }
    globalMatrix *= this->GetLocalMatrix();

    return globalMatrix;
}

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
