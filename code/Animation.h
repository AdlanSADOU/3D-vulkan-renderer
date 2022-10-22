#pragma once

struct TranslationChannel
{
    float     *timestamps;
    glm::vec3 *translations;
};
struct RotationChannel
{
    float     *timestamps;
    glm::quat *rotations;
};
struct ScaleChannel
{
    float     *timestamps;
    glm::vec3 *scales;
};

struct Sample
{
    uint8_t            target_joint;
    TranslationChannel translation_channel;
    RotationChannel    rotation_channel;
    ScaleChannel       scale_channel;
};

struct Joint
{
    char     *name;
    int8_t    parent; // 128 max joints!!
    glm::mat4 inv_bind_matrix;
    glm::mat4 global_joint_matrix;
};

struct AnimationV2
{
    float               duration;
    std::vector<Sample> samples;
};


AnimationV2 animation;

struct Animation
{
    void       *handle      = {};
    const char *name        = {};
    float       duration    = {};
    float       globalTimer = {};
    bool        isPlaying {};

    std::vector<Joint> _joints;

    std::vector<glm::mat4> joint_matrices;
};