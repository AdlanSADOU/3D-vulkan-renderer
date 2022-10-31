#pragma once

struct ObjectData
{
    glm::mat4 model;
    glm::mat4 joint_matrices[128];
};


struct PushConstants
{
    int32_t object_idx = -1;
    int32_t has_joints = -1;
    int32_t pad[2];
};


struct Transform
{
    const char *name        = {};
    Transform  *parent      = {};
    Transform  *child       = {};
    glm::vec3   scale       = {};
    glm::quat   rotation    = {};
    glm::vec3   translation = {};

    glm::mat4 GetLocalMatrix();
    // glm::mat4 ComputeGlobalMatrix();
};


// glm::mat4 Transform::ComputeGlobalMatrix()
// {
//     if (!parent) return GetLocalMatrix();

//     glm::mat4  globalMatrix = glm::mat4(1);
//     Transform *tmp          = this;

//     // ummm... yeah, this will be changed eventually
//     // it's the first iterative solution I came up with..
//     // turns out it is VERY terrible, not actually using it
//     std::list<Transform *> hierarchy;
//     while (tmp->parent) {
//         tmp = tmp->parent;
//         hierarchy.push_front(tmp);
//     }

//     for (auto &&i : hierarchy) {
//         globalMatrix *= i->GetLocalMatrix();
//     }
//     globalMatrix *= this->GetLocalMatrix();

//     return globalMatrix;
// }

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