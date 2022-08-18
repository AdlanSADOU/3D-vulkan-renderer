#pragma once

#include "mymath.h"

//camera
struct Camera
{
    glm::vec3 position;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 forward;
    float    pitch;
    float    yaw;
    float    roll;
};