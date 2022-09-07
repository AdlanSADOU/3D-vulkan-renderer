#pragma once


// camera
struct Camera
{
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
    float     _speed       = 20.f;
    float     _sensitivity = .046f;


    void CameraCreate(glm::vec3 position, float fov, float aspect, float near, float far);
    void CameraUpdate(Input &input, float dt);
};
static Camera *gCameraInUse;

void Camera::CameraCreate(glm::vec3 position, float fov, float aspect, float near, float far)
{
    _forward  = { 0, 0, -1 };
    _up       = { 0, 1, 0 };
    _yaw      = -90;
    _pitch    = 0;
    _fov      = fov;
    _aspect   = aspect;
    _near     = near;
    _far      = far;
    _position = position;

    _at         = _position + _forward;
    _projection = glm::perspective(Radians(_fov), _aspect, _near, _far);
    _view       = glm::lookAt(_position, _at, _up);
}

void Camera::CameraUpdate(Input &input, float dt)
{

    if (input.up) _position += _forward * _speed * dt;
    if (input.down) _position -= _forward * _speed * dt;
    if (input.left) _position -= glm::normalize(glm::cross(_forward, _up)) * _speed * dt;
    if (input.right) _position += glm::normalize(glm::cross(_forward, _up)) * _speed * dt;

    float factor = 0;
    if (input.E) factor += 1.f;
    if (input.Q) factor -= 1.f;
    _position.y += factor * dt / _sensitivity;

    static float xrelPrev = 0;
    static float yrelPrev = 0;
    int          xrel;
    int          yrel;

    SDL_GetRelativeMouseState(&xrel, &yrel);
    if (input.mouse.right) {
        SDL_SetRelativeMouseMode(SDL_TRUE);

        if (xrelPrev != xrel) _yaw += (float)xrel * _sensitivity;
        if (yrelPrev != yrel) _pitch -= (float)yrel * _sensitivity;
    } else
        SDL_SetRelativeMouseMode(SDL_FALSE);

    _forward.x = cosf(Radians(_yaw)) * cosf(Radians(_pitch));
    _forward.y = sinf(Radians(_pitch));
    _forward.z = sinf(Radians(_yaw)) * cosf(Radians(_pitch));
    _forward   = glm::normalize(_forward);
    xrelPrev   = xrel;
    yrelPrev   = yrel;

    _at         = _position + _forward;
    _projection = glm::perspective(_fov, _aspect, _near, _far);
    _view       = glm::lookAt(_position, _at, _up);
}
