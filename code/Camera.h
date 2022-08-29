#pragma once

#include <GLM/glm.hpp>

//camera
struct Camera
{
    glm::vec3 position   = {};
    glm::vec3 up         = {};
    glm::vec3 right      = {};
    glm::vec3 forward    = {};
    glm::vec3 at         = {};
    glm::mat4 projection = {};
    glm::mat4 view       = {};
    float     pitch      = {};
    float     yaw        = {};
    float     roll       = {};
    float     fov        = {};
    float     aspect     = {};
    float     near       = {};
    float     far        = {};
};
static Camera *gCameraInUse;

static void CameraCreate(Camera *camera, glm::vec3 position, float fov, float aspect, float near, float far)
{
    camera->forward  = { 0, 0, -1 };
    camera->up       = { 0, 1, 0 };
    camera->yaw      = -90;
    camera->pitch    = 0;
    camera->fov      = fov;
    camera->aspect   = aspect;
    camera->near     = near;
    camera->far      = far;
    camera->position = position;

    glm::vec3 at         = camera->position + camera->forward;
    glm::mat4 projection = glm::perspective(Radians(fov), aspect, near, far);
    glm::mat4 view       = glm::lookAt(camera->position, at, camera->up);
}

static void CameraUpdate()
{
    Camera c                 = *gCameraInUse;
    gCameraInUse->at         = gCameraInUse->position + gCameraInUse->forward;
    gCameraInUse->projection = glm::perspective(gCameraInUse->fov, gCameraInUse->aspect, gCameraInUse->near, gCameraInUse->far);
    gCameraInUse->view       = glm::lookAt(gCameraInUse->position, gCameraInUse->at, gCameraInUse->up);
}
