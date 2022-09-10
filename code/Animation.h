#pragma once

/**
 * updateCurrentAnimation(double globalTime)
 *      updateBone(size_t index, double time, glm::mat4 parentMatrix)
 *          calcScale(const double& time, const usls::scene::animation::Channel& channel)
 *          calcRotation(const double& time, const usls::scene::animation::Channel& channel)
 *          calcTranslation(const double& time, const usls::scene::animation::Channel& channel)
 *
 */
struct Joint
{
    char     *name;
    int8_t    parent;
    glm::mat4 invBindPose;
};

struct Skeleton
{
    uint32_t           id;
    uint32_t           jointCount;
    std::vector<Joint> joints;
};

struct AnimatedJoint
{
    uint32_t               xFormsCount;
    std::vector<Transform> xFormsPerFrame;
};

struct AnimationClip
{
    uint32_t                   id;
    float                      duration;
    std::vector<AnimatedJoint> joints;
};


//
// Sonme utility functions
//
static float AnimationGetClipDuration(cgltf_animation *animationClip)
{
    float duration = 0;

    for (size_t i = 0; i < animationClip->samplers_count; i++) {
        cgltf_accessor *input   = animationClip->samplers[i].input;
        static float    lastMax = 0;

        for (size_t j = 0; j < NB_OF_ELEMENTS_IN_ARRAY(input->max); j++) {
            if (input->has_max && input->max[j] > lastMax)
                lastMax = input->max[j];
        }
        duration = lastMax;
    }
    return duration;
}

static float readFloatFromAccessor(cgltf_accessor *accessor, cgltf_size idx)
{
    float value;
    if (cgltf_accessor_read_float(accessor, idx, &value, accessor->count))
        return value;
    else
        return 0;
}

static glm::vec3 getVec3AtKeyframe(cgltf_animation_sampler *sampler, int keyframe)
{
    // todo: check for failure?
    void *values = (void *)((uint8_t *)sampler->output->buffer_view->buffer->data + sampler->output->buffer_view->offset);
    return ((glm::vec3 *)values)[keyframe];
}
static glm::quat getQuatAtKeyframe(cgltf_animation_sampler *sampler, int keyframe)
{
    // todo: check for failure?
    void *values = (void *)((uint8_t *)sampler->output->buffer_view->buffer->data + sampler->output->buffer_view->offset);
    return ((glm::quat *)values)[keyframe];
}

/////////////////////////////////////



std::vector<glm::mat4> finalPoseJointMatrices;
std::vector<glm::mat4> currentPoseJointMatrices;
std::vector<Transform> currentPoseJointTransforms;


// note: this is unsued right now
std::vector<Transform> bindPoseLocalJointTransforms;
static void            ComputeLocalJointTransforms(cgltf_data *data)
{
    cgltf_node **joints = data->skins->joints;
    bindPoseLocalJointTransforms.resize(data->skins->joints_count);

    for (size_t joint_idx = 0; joint_idx < data->skins->joints_count; joint_idx++) {
        bindPoseLocalJointTransforms[joint_idx].translation = *(glm::vec3 *)joints[joint_idx]->translation;
        glm::vec3 R                                         = glm::eulerAngles(*(glm::quat *)joints[joint_idx]->rotation);
        bindPoseLocalJointTransforms[joint_idx].rotation    = R;
        bindPoseLocalJointTransforms[joint_idx].scale       = 1.f;

        bindPoseLocalJointTransforms[joint_idx].name = joints[joint_idx]->name;

        if (joint_idx == 0) continue;

        if (joints[joint_idx]->parent == joints[joint_idx - 1]) {
            bindPoseLocalJointTransforms[joint_idx].parent       = &bindPoseLocalJointTransforms[joint_idx - 1];
            bindPoseLocalJointTransforms[joint_idx].parent->name = bindPoseLocalJointTransforms[joint_idx - 1].name;

            bindPoseLocalJointTransforms[joint_idx - 1].child       = &bindPoseLocalJointTransforms[joint_idx];
            bindPoseLocalJointTransforms[joint_idx - 1].child->name = bindPoseLocalJointTransforms[joint_idx].name;
        } else {
            // find index of parent joint
            auto currentJointParent = joints[joint_idx]->parent;
            for (size_t i = 0; i < data->skins->joints_count; i++) {
                if (joints[i] == currentJointParent) {
                    bindPoseLocalJointTransforms[joint_idx].parent       = &bindPoseLocalJointTransforms[i];
                    bindPoseLocalJointTransforms[joint_idx].parent->name = bindPoseLocalJointTransforms[i].name;
                }
            }
        }
    }


    // for (size_t joint_idx = 0; joint_idx < data->skins->joints_count; joint_idx++) {

    //     cgltf_node *parent = NULL;
    //     Transform  *Tr     = NULL;


    //     SDL_Log("\n-- Joint[%s]: --", data->skins->joints[joint_idx]->name);
    //     SDL_Log("-----------TR-------------");
    //     if ((Tr = bindPoseLocalJointTransforms[joint_idx].parent) != NULL)
    //         for (size_t i = 0; Tr;) {
    //             SDL_Log("Child[%s]:", Tr->name);
    //             Tr = Tr->parent;
    //         }
    //     SDL_Log("-----------NODE-------------");

    //     if ((parent = data->skins->joints[joint_idx]->parent) != NULL)
    //         for (size_t i = 0; parent;) {
    //             SDL_Log("Child[%s]:", parent->name);
    //             parent = parent->parent;
    //         }
    // }
}






static void AnimationUpdate(float dt, cgltf_data *data)
{
    static float duration = AnimationGetClipDuration(&data->animations[0]);
    assert(duration > 0 && duration);

    static float globalTime = 0;
    globalTime += dt;
    float animTime = fmodf(globalTime, duration);

    const uint8_t jointCount = data->skins->joints_count;
    auto          joints     = data->skins->joints;

    finalPoseJointMatrices.resize(jointCount);
    currentPoseJointMatrices.resize(jointCount);
    currentPoseJointTransforms.resize(jointCount);


    assert(data->skins_count == 1);
    cgltf_animation *animation = &data->animations[0];

    static int iterationNb = 0;
    if (iterationNb++ == 0)
        SDL_Log("playing:[%s] | duration: %fsec", animation->name, duration);


    // For each Joint
    for (size_t joint_idx = 0; joint_idx < jointCount; joint_idx++) {

        // For each Channel
        for (size_t chan_idx = 0; chan_idx < animation->channels_count; chan_idx++) {
            cgltf_animation_channel *channel = &animation->channels[chan_idx];

            // if channel target matches current joint
            if (channel->target_node == joints[joint_idx]) { // && target_path!!!

                int currentKey = -1;
                int nextKey    = -1;

                for (size_t timestamp_idx = 0; timestamp_idx < channel->sampler->input->count - 1; timestamp_idx++) {
                    cgltf_animation_sampler *sampler = channel->sampler;
                    float                    sampled_time;
                    float                    sampled_time_prev;

                    if ((sampled_time = readFloatFromAccessor(sampler->input, timestamp_idx + 1)) > animTime) {
                        currentKey = timestamp_idx;
                        nextKey    = currentKey + 1;
                        break;
                    }
                }

                assert(currentKey >= 0 && nextKey >= 0);

                cgltf_animation_sampler *sampler = channel->sampler;

                // todo: currentKey => nextKey interpolation
                // use slerp for quaternions
                if (channel->target_path == cgltf_animation_path_type_translation) {
                    currentPoseJointTransforms[joint_idx].translation = getVec3AtKeyframe(sampler, currentKey);
                } else if (channel->target_path == cgltf_animation_path_type_rotation) {
                    auto Q                                         = getQuatAtKeyframe(sampler, currentKey);
                    currentPoseJointTransforms[joint_idx].rotation = glm::eulerAngles(Q);
                } else if (channel->target_path == cgltf_animation_path_type_scale) {
                    currentPoseJointTransforms[joint_idx].scale = 1.f;
                }

                currentPoseJointTransforms[joint_idx].name = joints[joint_idx]->name;



                if (joint_idx == 0) continue;

                if (joints[joint_idx]->parent == joints[joint_idx - 1]) {
                    currentPoseJointTransforms[joint_idx].parent       = &currentPoseJointTransforms[joint_idx - 1];
                    currentPoseJointTransforms[joint_idx].parent->name = currentPoseJointTransforms[joint_idx - 1].name;

                    // currentPoseJointTransforms[joint_idx - 1].child       = &currentPoseJointTransforms[joint_idx];
                    // currentPoseJointTransforms[joint_idx - 1].child->name = currentPoseJointTransforms[joint_idx].name;
                } else {
                    // find index of parent joint
                    auto currentJointParent = joints[joint_idx]->parent;
                    for (size_t i = 0; i < data->skins->joints_count; i++) {
                        if (joints[i] == currentJointParent) {
                            currentPoseJointTransforms[joint_idx].parent       = &currentPoseJointTransforms[i];
                            currentPoseJointTransforms[joint_idx].parent->name = currentPoseJointTransforms[i].name;
                        }
                    }
                }
            }
        } // End of channels loop for the current joint




        // comppute global matrix for current joint
        if (joint_idx == 0) {
            currentPoseJointMatrices[0] = currentPoseJointTransforms[0].GetLocalMatrix();
        } else {
            currentPoseJointMatrices[joint_idx] = currentPoseJointTransforms[joint_idx].ComputeGlobalMatrix();
        }

        glm::mat4 *invBindMatrices        = ((glm::mat4 *)((uint8_t *)data->skins->inverse_bind_matrices->buffer_view->buffer->data + data->skins->inverse_bind_matrices->buffer_view->offset));
        finalPoseJointMatrices[joint_idx] = currentPoseJointMatrices[joint_idx] * invBindMatrices[joint_idx];
        // finalPoseJointMatrices[joint_idx]   = currentPoseJointMatrices[joint_idx] * glm::inverse(bindPoseLocalJointTransforms[joint_idx].ComputeGlobalMatrix());
    } // End of Joints loop
}



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////


// static void AnimationUpdate(AnimationClip clip, float globalTime)
// {
//     float frameTime = globalTime * clip.framerate;
//     float time = fmodf(frameTime, clip.duration);

//     UpdateBone(0, time, rootTransform);
// }
