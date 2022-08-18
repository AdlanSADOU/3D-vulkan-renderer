#pragma once

#include "mymath.h"

//camera
struct Camera
{
    hmm_vec3 position;
    hmm_vec3 up;
    hmm_vec3 right;
    hmm_vec3 forward;
    float    pitch;
    float    yaw;
    float    roll;
};