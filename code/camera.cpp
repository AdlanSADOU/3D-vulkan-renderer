
void Camera::CameraCreate(glm::vec3 position, float fov, float aspect, float near, float far)
{
    _forward  = { 0, 0, -1 };
    _right    = {};
    _up       = { 0, 1, 0 };
    _yaw      = 0;
    _pitch    = 0;
    _fov      = fov;
    _aspect   = 16 / (float)9;
    _near     = near;
    _far      = far;
    _position = position;

    _at         = _position + _forward;
    _projection = glm::perspective(glm::radians(_fov), _aspect, _near, _far);
    _view       = glm::lookAt(_position, _at, _up);

    mode = FREEFLY;
}

void Camera::CameraUpdate(Input *input, float dt, glm::vec3 target)
{
    switch (mode) {
        case FREEFLY:
            {
                if (input->up) _position += _forward * _speed * dt;
                if (input->down) _position -= _forward * _speed * dt;
                if (input->left) _position -= glm::normalize(glm::cross(_forward, _up)) * _speed * dt;
                if (input->right) _position += glm::normalize(glm::cross(_forward, _up)) * _speed * dt;

                float factor = 0;
                if (input->E) factor -= 1.f;
                if (input->Q) factor += 1.f;
                _position.y += factor * dt / _sensitivity;

                static float xrelPrev = 0;
                static float yrelPrev = 0;
                int          xrel;
                int          yrel;

                SDL_GetRelativeMouseState(&xrel, &yrel);
                if (input->mouse.right) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);

                    if (xrelPrev != xrel) _yaw += (float)xrel * _sensitivity;
                    if (yrelPrev != yrel) _pitch -= (float)yrel * _sensitivity;
                } else
                    SDL_SetRelativeMouseMode(SDL_FALSE);

                _forward.x = cosf(glm::radians(_yaw)) * cosf(glm::radians(_pitch));
                _forward.y = sinf(glm::radians(_pitch));
                _forward.z = sinf(glm::radians(_yaw)) * cosf(glm::radians(_pitch));
                _forward   = glm::normalize(_forward);
                xrelPrev   = xrel;
                yrelPrev   = yrel;

                _at   = _position + _forward;
                _view = glm::lookAt(_position, _at, _up);
            }
            break;

        case THIRD_PERSON:
            {

                float factor = 0;
                if (input->E) factor -= 1.f;
                if (input->Q) factor += 1.f;
                _position.y += factor * dt / _sensitivity;

                static float xrelPrev = 0;
                static float yrelPrev = 0;
                int          xrel;
                int          yrel;

                SDL_GetRelativeMouseState(&xrel, &yrel);
                if (input->mouse.right) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);

                    if (xrelPrev != xrel) _yaw += (float)xrel * _sensitivity;
                    if (yrelPrev != yrel) _pitch -= (float)yrel * _sensitivity;
                } else
                    SDL_SetRelativeMouseMode(SDL_FALSE);



                static glm::vec3 offset {};
                offset.x = cosf(glm::radians(_yaw)) * cosf(glm::radians(_pitch)) * 60;
                offset.y = sinf(glm::radians(_pitch)) * 60 * -1;
                offset.z = sinf(glm::radians(_yaw)) * cosf(glm::radians(_pitch)) * 60;
                _forward = (offset);
                _right   = (glm::cross(_forward, _up));

                xrelPrev = xrel;
                yrelPrev = yrel;

                _position = target + offset;
                _view     = glm::lookAt(_position, target + glm::vec3(0, 20, 0), _up);
            }
            break;

        default:
            break;
    }

    _projection = glm::perspective(_fov, _aspect, _near, _far);
}
