#pragma once

static bool LoadCgltfData(const char *path, cgltf_data **data)
{
    cgltf_options options = {};
    cgltf_result  result  = cgltf_parse_file(&options, path, &(*data));

    if (cgltf_parse_file(&options, path, &(*data)) == cgltf_result_success) {
        if (cgltf_load_buffers(&options, (*data), path) == cgltf_result_success) {
            if ((result = cgltf_validate((*data))) != cgltf_result_success) {
                SDL_Log("cgltf validation error");
                return false;
            }
            result = cgltf_load_buffers(&options, (*data), path);
        }
    } else {
        SDL_Log("Failed to load [%s]", path);
        return false;
    }

    return true;
}

// Callee allocates dst
static errno_t GetPathFolder(char **dst, const char *src, uint32_t srcLength)
{
    assert(src);
    assert(srcLength > 0);

    size_t size = std::string(src).find_last_of('/');
    *dst        = (char *)malloc(sizeof(char) * size + 1);
    return strncpy_s(*(char **)dst, size + 2, src, size + 1);
}



//
// Sonme utility functions
//

static float AnimationGetClipDuration(cgltf_animation *animationClip)
{
    float duration = 0;
    float lastMax  = 0;

    for (size_t i = 0; i < animationClip->samplers_count; i++) {
        cgltf_accessor *input = animationClip->samplers[i].input;

        for (size_t j = 0; j < ARR_COUNT(input->max); j++) {
            if (input->has_max && input->max[j] > lastMax)
                lastMax = input->max[j];
        }
        duration = lastMax;
    }
    return duration;
}

static int AnimationMaxSampleCount(cgltf_animation *animationClip)
{
    float last_max = 0;

    for (size_t i = 0; i < animationClip->samplers_count; i++) {
        auto *input = animationClip->samplers[i].input;

        if (input->count > last_max) {
            last_max = input->count;
        }
    }
    return last_max;
}

static float readFloatFromAccessor(cgltf_accessor *accessor, cgltf_size idx)
{
    float value;
    // SDL_Log("idx:[%d]", idx);
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
