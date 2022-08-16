#pragma once

#include "mymath.h"

//camera
struct Camera
{
    Vector3 position;
    Vector3 up;
    Vector3 right;
    Vector3 forward;
    float   pitch;
    float   yaw;
    float   roll;
};