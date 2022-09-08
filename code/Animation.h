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

static cgltf_data *LoadGltfData(const char *skeletonPath)
{
    cgltf_options opts {};
    cgltf_data   *data {};
    cgltf_result  res {};

    if (cgltf_parse_file(&opts, skeletonPath, &data) == cgltf_result_success) {
        if (cgltf_load_buffers(&opts, data, skeletonPath) == cgltf_result_success) {
            if ((res = cgltf_validate(data)) != cgltf_result_success) {
                SDL_Log("cgltf validation error");
            }
        }
    }

    assert(data);
    return data;
}



// should we interpolate and cache sampler ouputs
// into poses we can use?

static float *GetAnimatedPropertyChannelAtFrame(uint32_t atFrame, cgltf_animation_channel *channel, cgltf_animation_path_type property, float *outTime = NULL)
{
    cgltf_accessor *input          = channel->sampler->input;
    cgltf_accessor *output         = channel->sampler->output;
    uint32_t        componentCount = (output->buffer_view->size / output->count / cgltf_component_size(output->component_type));
    float          *values         = ((float *)((uint8_t *)channel->sampler->output->buffer_view->buffer->data + channel->sampler->output->buffer_view->offset));
    float          *timeValues     = (float *)((uint8_t *)input->buffer_view->buffer->data + input->buffer_view->offset);

    // note: we could even avoid iterating, because we know what frame we want
    // we just have to be carefull and check if the frame exists
    for (size_t frames = 0; frames < input->count; frames++) {
        float    time  = timeValues[frames];
        uint32_t frame = time * 60;
        if (frame == atFrame) {
            if (outTime) *outTime = time;
            return values + componentCount * frames;
        }
    }
    return 0;
}


static float AnimationGetClipDuration(cgltf_animation *animationClip)
{
    float duration = 0;

    for (size_t i = 0; i < animationClip->samplers_count; i++) {
        cgltf_accessor *input   = animationClip->samplers[i].input;
        static float    lastMax = 0;

        for (size_t j = 0; j < NB_OF_ELEMENTS_IN_ARRAY(input->max); j++) {
            if (input->has_max && input->max[i] > lastMax)
                lastMax = input->max[i];
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





std::vector<glm::mat4> jointMatrices;
std::vector<glm::mat4> globalJointMatrices;
std::vector<Transform> jointTransforms;
std::vector<Transform> localJointTransforms;

static void ComputeLocalJointTransforms(cgltf_data *data)
{
    cgltf_node **joints = data->skins->joints;
    localJointTransforms.resize(data->skins->joints_count);

    for (size_t joint_idx = 0; joint_idx < data->skins->joints_count; joint_idx++) {
        localJointTransforms[joint_idx].translation = *(glm::vec3 *)joints[joint_idx]->translation;
        glm::vec3 R                                 = glm::eulerAngles(*(glm::quat *)joints[joint_idx]->rotation);
        localJointTransforms[joint_idx].rotation    = R;
        localJointTransforms[joint_idx].scale       = 1.f;

        localJointTransforms[joint_idx].name = joints[joint_idx]->name;

        if (joint_idx == 0) continue;

        if (joints[joint_idx]->parent == joints[joint_idx - 1]) {
            localJointTransforms[joint_idx].parent       = &localJointTransforms[joint_idx - 1];
            localJointTransforms[joint_idx].parent->name = localJointTransforms[joint_idx - 1].name;

            localJointTransforms[joint_idx - 1].child       = &localJointTransforms[joint_idx];
            localJointTransforms[joint_idx - 1].child->name = localJointTransforms[joint_idx].name;
        } else {
            // find index of parent joint
            auto currentJointParent = joints[joint_idx]->parent;
            for (size_t i = 0; i < data->skins->joints_count; i++) {
                if (joints[i] == currentJointParent) {
                    localJointTransforms[joint_idx].parent       = &localJointTransforms[i];
                    localJointTransforms[joint_idx].parent->name = localJointTransforms[i].name;
                }
            }
        }
    }


    // for (size_t joint_idx = 0; joint_idx < data->skins->joints_count; joint_idx++) {

    //     cgltf_node *parent = NULL;
    //     Transform  *Tr     = NULL;


    //     SDL_Log("\n-- Joint[%s]: --", data->skins->joints[joint_idx]->name);
    //     SDL_Log("-----------TR-------------");
    //     if ((Tr = localJointTransforms[joint_idx].parent) != NULL)
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





static int       i = 0;
static glm::mat4 UpdateGlobalMatrix(Transform *t, glm::mat4 m)
{
    // SDL_Log("%d", i++);
    if (!t->parent) {
        i = 0;
        return glm::mat4(1);
    }
    SDL_Log("[%d]: %s * %s", i++, t->parent->name, t->name);

    return UpdateGlobalMatrix(t->parent, t->parent->GetLocalMatrix() * t->GetLocalMatrix());
}





static void AnimationUpdate(float dt, cgltf_data *data)
{
    static float duration   = AnimationGetClipDuration(&data->animations[0]);
    static float globalTime = 0;
    globalTime += dt;
    float animTime = fmodf(globalTime, duration);

    const uint8_t jointCount = data->skins->joints_count;
    auto          joints     = data->skins->joints;

    jointMatrices.resize(jointCount);
    globalJointMatrices.resize(jointCount);
    jointTransforms.resize(jointCount);


    assert(data->skins_count == 1);
    cgltf_animation *animation = &data->animations[0];

    static int iterationNb = 0;
    if (iterationNb++ == 0)
        SDL_Log("playing:[%s]", animation->name);


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
                if (currentKey < 0 || nextKey < 0) {
                    SDL_Log("invalid keyframe");
                    return;
                }

                cgltf_animation_sampler *sampler = channel->sampler;

                // todo: currentKey => nextKey interpolation
                // use slerp for quaternions
                if (channel->target_path == cgltf_animation_path_type_translation) {
                    jointTransforms[joint_idx].translation = getVec3AtKeyframe(sampler, currentKey);
                } else if (channel->target_path == cgltf_animation_path_type_rotation) {
                    auto Q                              = getQuatAtKeyframe(sampler, currentKey);
                    jointTransforms[joint_idx].rotation = glm::eulerAngles(Q);
                } else if (channel->target_path == cgltf_animation_path_type_scale) {
                    jointTransforms[joint_idx].scale = 1.f;
                }

                jointTransforms[joint_idx].name = joints[joint_idx]->name;



                if (joint_idx == 0) continue;

                if (joints[joint_idx]->parent == joints[joint_idx - 1]) {
                    jointTransforms[joint_idx].parent       = &jointTransforms[joint_idx - 1];
                    jointTransforms[joint_idx].parent->name = jointTransforms[joint_idx - 1].name;

                    // jointTransforms[joint_idx - 1].child       = &jointTransforms[joint_idx];
                    // jointTransforms[joint_idx - 1].child->name = jointTransforms[joint_idx].name;
                } else {
                    // find index of parent joint
                    auto currentJointParent = joints[joint_idx]->parent;
                    for (size_t i = 0; i < data->skins->joints_count; i++) {
                        if (joints[i] == currentJointParent) {
                            jointTransforms[joint_idx].parent       = &jointTransforms[i];
                            jointTransforms[joint_idx].parent->name = jointTransforms[i].name;
                        }
                    }
                }
            }
        } // End of channels loop for the current joint




        // comppute global matrix for current joint
        if (joint_idx == 0) {
            globalJointMatrices[0] = jointTransforms[0].GetLocalMatrix();
        } else {
            globalJointMatrices[joint_idx] = jointTransforms[joint_idx].ComputeGlobalMatrix();
        }

        glm::mat4 *invBindMatrices = ((glm::mat4 *)((uint8_t *)data->skins->inverse_bind_matrices->buffer_view->buffer->data + data->skins->inverse_bind_matrices->buffer_view->offset));
        jointMatrices[joint_idx]   = globalJointMatrices[joint_idx] * invBindMatrices[joint_idx];
        // jointMatrices[joint_idx]   = globalJointMatrices[joint_idx] * glm::inverse(localJointTransforms[joint_idx].ComputeGlobalMatrix());
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
