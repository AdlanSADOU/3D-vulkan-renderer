#pragma once


// camera
struct Camera
{
    enum CameraMode {
        FREEFLY = 0,
        THIRD_PERSON
    } mode;


    glm::vec3 _position    = {};
    glm::vec3 _up          = {};
    glm::vec3 _right       = {};
    glm::vec3 _forward     = {};
    glm::vec3 _at          = {};
    glm::mat4 _projection  = {};
    glm::mat4 _view        = {};
    float     _pitch       = {};
    float     _yaw         = {};
    float     _roll        = {};
    float     _fov         = {};
    float     _aspect      = {};
    float     _near        = {};
    float     _far         = {};
    float     _speed       = 60.f;
    float     _sensitivity = .046f;


    void CameraCreate(glm::vec3 position, float fov, float aspect, float near, float far);
    void CameraUpdate(Input *input, float dt, glm::vec3 target);
};
