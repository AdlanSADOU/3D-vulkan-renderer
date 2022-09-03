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

static float *GetAnimatedPropertyChannel(cgltf_animation_channel *channel, cgltf_animation_path_type property, uint32_t atFrame)
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
        uint32_t frame = time * 30;
        if (frame == atFrame) {
            return values + componentCount * frames;
        }
    }
    return 0;
}


static void PrintAnimationClipTransforms()
{
    cgltf_data *data = LoadGltfData("assets/test_rig.gltf");
    assert(data->animations_count > 0);

    AnimationClip clip;
    clip.joints.resize(data->skins[0].joints_count);

    for (size_t joint_idx = 0; joint_idx < data->skins[0].joints_count; joint_idx++) {
        cgltf_node **joints = data->skins[0].joints;

        glm::vec3 T {};
        glm::quat Q {};

        // it would have been much more practical if channels could be indexed by bone indices
        // and each channel per bone would contain SQT values for each frame
        // channel[bone_id].translation
        // channel[bone_id].rotation
        // channel[bone_id].scale
        for (size_t frameCounter = 1; frameCounter < 6; frameCounter++) {
            /* code */

            for (size_t channel_idx = 0; channel_idx < data->animations[0].channels_count; channel_idx++) {
                cgltf_animation_channel *channel = &data->animations[0].channels[channel_idx];

                if (joints[joint_idx] == channel->target_node) {
                    if (channel->target_path == cgltf_animation_path_type_translation) {
                        float *values = GetAnimatedPropertyChannel(channel, cgltf_animation_path_type_translation, frameCounter);
                        if (values) {
                            T = *(glm::vec3 *)values;
                        }
                    }
                    if (channel->target_path == cgltf_animation_path_type_rotation) {
                        float *values = GetAnimatedPropertyChannel(channel, cgltf_animation_path_type_rotation, frameCounter);
                        if (values) {
                            Q = *(glm::quat *)values;
                        }
                    }
                }
            }
            glm::vec3 R = glm::eulerAngles(Q);
            SDL_Log("joint:[%s] frame[%zu] T(%.2f, %.2f, %.2f) R(%.2f, %.2f, %.2f)", joints[joint_idx]->name, frameCounter, T.x, T.y, T.z, Degrees(R.x), Degrees(R.y), Degrees(R.z));
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
