#pragma once

/**
 * updateCurrentAnimation(double globalTime)
 *      updateBone(size_t index, double time, glm::mat4 parentMatrix)
 *          calcScale(const double& time, const usls::scene::animation::Channel& channel)
 *          calcRotation(const double& time, const usls::scene::animation::Channel& channel)
 *          calcTranslation(const double& time, const usls::scene::animation::Channel& channel)
 *
 *          get the channel
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

    // if (res == cgltf_result_success) {
    //     for (size_t i = 0; i < data->animations_count; i++) {
    //         for (size_t j = 0; j < data->animations[i].channels_count; j++) {
    //             cgltf_animation_channel *channel = data->animations[i].channels + j;

    //             // float *input = (float*)((uint8_t *)channel->sampler->input->buffer_view->buffer->data + channel->sampler->input->buffer_view->offset);
    //             // SDL_Log("frame %f", input);
    //             float input;
    //             for (size_t in_count = 0; in_count < channel->sampler->input->count; in_count++) {
    //                 cgltf_accessor_read_float(channel->sampler->input, in_count, &input, 1);
    //                 SDL_Log("[%s][%s] frametime :%f", channel->target_node->name, cgltf_animation_path_type_names[channel->target_path], input);
    //             }


    //             channel->sampler->output->buffer_view->offset;
    //             channel->sampler->output->buffer_view->buffer->data;
    //         }
    //     }
    // }
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

static void PrintAnimationClipTransformsByFrame(cgltf_data *data, uint32_t frame)
{
    assert(data->animations_count > 0);

    AnimationClip clip;
    clip.joints.resize(data->skins[0].joints_count);

    // find the duration of this clip
    // which is the maxium max value of all sampler->input->max
    uint32_t frameCount = data->animations[0].samplers[0].input->count;
    float    duration   = AnimationGetClipDuration(&data->animations[0]);

    SDL_Log("duration:[%f] | frames/samples:[%d]", duration, frameCount);

    for (size_t joint_idx = 0; joint_idx < data->skins[0].joints_count; joint_idx++) {
        cgltf_node **joints = data->skins[0].joints;

        glm::vec3 T {};
        glm::quat Q {};
        float     frameTime = 0;
        // it would have been much more practical if channels could be indexed by bone indices
        // and each channel per bone would contain SQT values for each frame
        // channel[bone_id].translation
        // channel[bone_id].rotation
        // channel[bone_id].scale

        for (size_t frameCounter = 1; frameCounter <= frameCount; frameCounter++) {
            /* code */

            for (size_t channel_idx = 0; channel_idx < data->animations[0].channels_count; channel_idx++) {
                cgltf_animation_channel *channel = &data->animations[0].channels[channel_idx];

                if (joints[joint_idx] == channel->target_node) {
                    if (channel->target_path == cgltf_animation_path_type_translation) {

                        float *values = GetAnimatedPropertyChannelAtFrame(frameCounter, channel, cgltf_animation_path_type_translation, &frameTime);
                        if (values) {
                            T = *(glm::vec3 *)values;
                        }
                    }
                    if (channel->target_path == cgltf_animation_path_type_rotation) {
                        float *values = GetAnimatedPropertyChannelAtFrame(frameCounter, channel, cgltf_animation_path_type_rotation);
                        if (values) {
                            Q = *(glm::quat *)values;
                        }
                    }
                }
            }
            glm::vec3 R = glm::eulerAngles(Q);
            SDL_Log("joint:[%s] frame[%d] time[%f] T(%.2f, %.2f, %.2f) R(%.2f, %.2f, %.2f)",
                joints[joint_idx]->name, frame, frameTime,
                T.x, T.y, T.z,
                Degrees(R.x), Degrees(R.y), Degrees(R.z));
        }
    }

    // AnimationGetAnimatedProperty(&data->animations[0], )
}


// static void AnimationUpdate(AnimationClip clip, float globalTime)
// {
//     float frameTime = globalTime * clip.framerate;
//     float time = fmodf(frameTime, clip.duration);

//     UpdateBone(0, time, rootTransform);
// }

// *animation, *skin
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

static void ComputeLocalJointTransform(cgltf_data *data)
{
    cgltf_node **joints = data->skins->joints;
    localJointTransforms.resize(data->skins->joints_count);

    for (size_t i = 0; i < data->skins->joints_count; i++) {
        // if (joints[i]->has_translation)
        {
            localJointTransforms[i].translation = *(glm::vec3 *)joints[i]->translation;
        }
        // if (joints[i]->has_translation)
        {
            glm::vec3 R                      = glm::eulerAngles(*(glm::quat *)joints[i]->rotation);
            localJointTransforms[i].rotation = R;
        }
        // if (joints[i]->has_scale)
        {
            localJointTransforms[i].scale = 1.f;
        }
    }
}

static void AnimationUpdate(float dt, cgltf_data *data)
{
    static float duration   = AnimationGetClipDuration(&data->animations[0]);
    static float globalTime = 0;
    globalTime += dt;
    float animTime = fmodf(globalTime, duration);

    const uint8_t jointCount = data->skins[0].joints_count;

    jointMatrices.resize(jointCount);
    globalJointMatrices.resize(jointCount);
    jointTransforms.resize(jointCount);


    assert(data->skins_count == 1);
    cgltf_animation *animation = &data->animations[0];

    SDL_Log("playing:[%s]", animation->name);

    for (size_t joint_idx = 0; joint_idx < jointCount; joint_idx++) {
        for (size_t chan_idx = 0; chan_idx < animation->channels_count; chan_idx++) {
            if (animation->channels[chan_idx].target_node == data->skins->joints[joint_idx]) { // && target_path!!!

                int currentKey = -1;
                int nextKey    = -1;

                for (size_t timestamp_idx = 0; timestamp_idx < animation->channels[chan_idx].sampler->input->count - 1; timestamp_idx++) {
                    cgltf_animation_sampler *sampler = animation->channels[chan_idx].sampler;
                    float                    sampled_time;
                    float                    sampled_time_prev;


                    if ((sampled_time = readFloatFromAccessor(sampler->input, timestamp_idx + 1)) == animTime) {
                        // todo: if sampled frame time and current animation time are equal
                        // then take the SQTs as is. No interpolation needed
                        break;
                    }

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

                cgltf_animation_sampler *sampler = animation->channels[chan_idx].sampler;

                // todo: currentKey => nextKey interpolation
                // use slerp for quaternions
                if (animation->channels[chan_idx].target_path == cgltf_animation_path_type_translation) {
                    jointTransforms[joint_idx].translation = getVec3AtKeyframe(sampler, currentKey);
                } else if (animation->channels[chan_idx].target_path == cgltf_animation_path_type_rotation) {
                    auto Q                              = getQuatAtKeyframe(sampler, currentKey);
                    jointTransforms[joint_idx].rotation = glm::eulerAngles(Q);
                } else if (animation->channels[chan_idx].target_path == cgltf_animation_path_type_scale) {
                    jointTransforms[joint_idx].scale = 1.f;
                }
            }
        }

        // todo: we have to compute these matrices with the actual joint parents!!!
        // not just the joint order
        {
            if (joint_idx == 0) {
                globalJointMatrices[0] = jointTransforms[0].GetMatrix();
            } else {
                globalJointMatrices[joint_idx] = globalJointMatrices[joint_idx - 1] * jointTransforms[joint_idx].GetMatrix();
            }

            glm::mat4 *invBindMatrices = ((glm::mat4 *)((uint8_t *)data->skins->inverse_bind_matrices->buffer_view->buffer->data + data->skins->inverse_bind_matrices->buffer_view->offset));

            jointMatrices[joint_idx] = globalJointMatrices[joint_idx] * invBindMatrices[joint_idx];

            // SDL_Log("Joint[%s]:", data->skins->joints[joint_idx]->name);

            // cgltf_node *parent = NULL;
            // if ((parent = data->skins->joints[joint_idx]->parent) != NULL)
            //     for (size_t i = 0; parent;) {
            //         SDL_Log("Parent[%s]:", parent->name);
            //         parent = parent->parent;
            //     }
        }
    }

    // for (size_t i = 0; i < jointTransforms.size(); i++) {
    //     auto T = jointTransforms[i].translation;
    //     auto R = jointTransforms[i].rotation;

    //     SDL_Log("Joint[%s] time[%f] => T(%.2f, %.2f, %.2f) Q(%.2f, %.2f, %.2f))",
    //         data->skins->joints[i]->name, animTime,
    //         T.x, T.y, T.z, Degrees(R.x), Degrees(R.y), Degrees(R.z));
    // }

    // auto x = (void *)0;
}