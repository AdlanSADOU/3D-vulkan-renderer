#pragma once

#include <vector>

struct Joint
{
    char *    name;
    int8_t    parent;
    glm::mat4 invBindPose;
} * joints;

struct Skeleton
{
    uint32_t           id;
    uint32_t           jointCount;
    std::vector<Joint> joints;
};

/**
 * Is a pose sample actually an offset relative to
 * the rest pose or an offset relative to the previous pose...?
 * ITS NONE OF THAT
 */
struct Transform
{
    float     scale;
    glm::quat rotation;
    glm::vec3 translation;
};

struct AnimationFrame
{
    // transforms for each joint at that frame
    // [joint_0].transform.SQT
    // [joint_1].transform.SQT
    // [joint_2].transform.SQT
    std::vector<Transform> transforms;
};

struct AnimationClip
{
    uint32_t id;
    float    duration;
    // [frame_0].transforms[joint_0].SQT
    // [frame_0].transforms[joint_1].SQT
    // [frame_0].transforms[joint_2].SQT
    // [frame_1].transforms[joint_0].SQT
    // [frame_1].transforms[joint_1].SQT
    // [frame_1].transforms[joint_2].SQT
    // ...
    std::vector<AnimationFrame> frameSamples;
};

static void PrintAnimationClipTransforms()
{
    const char *  skeletonPath = "assets/test_rig.gltf";
    cgltf_options opts {};
    cgltf_data *  data {};
    cgltf_result  res {};

    if (cgltf_parse_file(&opts, skeletonPath, &data) == cgltf_result_success) {
        if (cgltf_load_buffers(&opts, data, skeletonPath) == cgltf_result_success) {
            if ((res = cgltf_validate(data)) != cgltf_result_success) {
                SDL_Log("cgltf validation error");
            }
        }
    }

    if (res == cgltf_result_success) {
        for (size_t i = 0; i < data->animations_count; i++) {
            for (size_t j = 0; j < data->animations[i].channels_count; j++) {
                cgltf_animation_channel *channel = data->animations[i].channels + j;

                // float *input = (float*)((uint8_t *)channel->sampler->input->buffer_view->buffer->data + channel->sampler->input->buffer_view->offset);
                // SDL_Log("frame %f", input);
                float input;
                for (size_t in_count = 0; in_count < channel->sampler->input->count; in_count++) {
                    cgltf_accessor_read_float(channel->sampler->input, in_count, &input, 1);
                    SDL_Log("[%s][%s] frametime :%f", channel->target_node->name, cgltf_animation_path_type_names[channel->target_path], input);
                }


                channel->sampler->output->buffer_view->offset;
                channel->sampler->output->buffer_view->buffer->data;
            }
        }
    }
}

// should we interpolate and cache sampler ouputs
// into poses we can use?

static float *AnimationGetAnimatedProperty(cgltf_animation *animation, float globalTime, cgltf_node *node, cgltf_animation_path_type property)
{
    for (size_t i = 0; i < animation->channels_count; i++) {
        cgltf_animation_channel *channel = animation->channels + i;

        // if our target node && property match
        // we proceed to find the right current & next key
        if (channel->target_node == node && channel->target_path == property) {
            float input;

            for (size_t in_count = 0; in_count < channel->sampler->input->count; in_count++) {
                cgltf_accessor_read_float(channel->sampler->input, in_count, &input, 1);
                SDL_Log("[%s][%s] frametime :%f", channel->target_node->name, cgltf_animation_path_type_names[channel->target_path], input);
            }
        }
        // float *input = (float*)((uint8_t *)channel->sampler->input->buffer_view->buffer->data + channel->sampler->input->buffer_view->offset);
        // SDL_Log("frame %f", input);


        channel->sampler->output->buffer_view->offset;
        channel->sampler->output->buffer_view->buffer->data;
    }
}