#pragma once

static uint32_t global_instance_id = 0;

static void LoadTexture(int32_t *texture_idx, const char *texture_uri, const char *folder_path)
{
    std::string full_path;
    full_path.append(folder_path);
    full_path.append(texture_uri);

    Texture *t = (Texture *)malloc(sizeof(Texture));
    t->Create(full_path.c_str());
    t->name = strdup(texture_uri);

    gTextures.insert({ std::string(texture_uri), t });

    *texture_idx = t->id;
}

struct SkinnedModel
{
    std::vector<Joint>       _joints;
    // std::vector<AnimationV2> _animations_v2;
    std::vector<Animation>   _animations;
    Animation               *_current_animation     = {};
    bool                     _should_play_animation = true;


    // todo: we must eventually cache this data to avoid
    // creating GPU buffers for the same Mesh multiple times
    VkBuffer      vertex_buffer;
    VmaAllocation vertex_buffer_allocation;
    cgltf_data   *_mesh_data;
    struct SkinnedMesh
    {
        uint32_t id;

        std::vector<MaterialData> material_data {};

        // offsets to non-interleaved vertex attributes
        std::vector<VkDeviceSize> POSITION_offset;
        std::vector<VkDeviceSize> NORMAL_offset;
        std::vector<VkDeviceSize> TEXCOORD_0_offset;
        std::vector<VkDeviceSize> TEXCOORD_1_offset;
        std::vector<VkDeviceSize> JOINTS_0_offset;
        std::vector<VkDeviceSize> WEIGHTS_0_offset;

        std::vector<VkDeviceSize> index_offset;
        std::vector<uint32_t>     index_count;
    };
    std::vector<SkinnedMesh> _meshes;


    void Create(const char *path);
    void Draw(VkCommandBuffer command_buffer, uint32_t frame_in_flight);
    void AnimationUpdate(float dt);
};

void SkinnedModel::Create(const char *path)
{
    if (gSharedMeshes.contains(path)) {
        _mesh_data = (cgltf_data *)gSharedMeshes[path];

    } else {
        cgltf_data *data;
        LoadCgltfData(path, &data);

        gSharedMeshes.insert({ std::string(path), (void *)data });
        _mesh_data = data;
    }


    char *folder_path;
    GetPathFolder(&folder_path, path, strlen(path));


    // size_t vertex_buffer_size = sizeof(triangle_vertices) + sizeof(triangle_indices);
    size_t vertex_buffer_size = _mesh_data->buffers->size;


    VkBufferCreateInfo buffer_ci {};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size  = vertex_buffer_size;
    buffer_ci.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_allocation_ci {};
    vma_allocation_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    vma_allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vmaCreateBuffer(gAllocator, &buffer_ci, &vma_allocation_ci, &vertex_buffer, &vertex_buffer_allocation, NULL);
    void *mapped_vertex_buffer_ptr;
    vmaMapMemory(gAllocator, vertex_buffer_allocation, &mapped_vertex_buffer_ptr);
    memcpy(mapped_vertex_buffer_ptr, _mesh_data->buffers->data, vertex_buffer_size);
    vmaUnmapMemory(gAllocator, vertex_buffer_allocation);



    _meshes.resize(_mesh_data->meshes_count);


    for (size_t mesh_idx = 0; mesh_idx < _mesh_data->meshes_count; mesh_idx++) {
        auto mesh             = &_meshes[mesh_idx];
        auto primitives_count = _mesh_data->meshes[mesh_idx].primitives_count;

        mesh->material_data.resize(primitives_count);
        mesh->POSITION_offset.resize(primitives_count);
        mesh->NORMAL_offset.resize(primitives_count);
        mesh->TEXCOORD_0_offset.resize(primitives_count);
        mesh->TEXCOORD_1_offset.resize(primitives_count);
        mesh->JOINTS_0_offset.resize(primitives_count);
        mesh->WEIGHTS_0_offset.resize(primitives_count);
        mesh->index_offset.resize(primitives_count);
        mesh->index_count.resize(primitives_count);


        for (size_t submesh_idx = 0; submesh_idx < primitives_count; submesh_idx++) {
            //
            // Vertex Attributes
            //
            cgltf_primitive *primitive = &_mesh_data->meshes[mesh_idx].primitives[submesh_idx];

            for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                cgltf_attribute   *attrib = &primitive->attributes[attrib_idx];
                cgltf_buffer_view *view   = attrib->data->buffer_view;

                if (attrib->type == cgltf_attribute_type_position)
                    mesh->POSITION_offset[submesh_idx] = view->offset;
                else if (attrib->type == cgltf_attribute_type_normal)
                    mesh->NORMAL_offset[submesh_idx] = view->offset;
                else if (attrib->type == cgltf_attribute_type_texcoord && strcmp(attrib->name, "TEXCOORD_0") == 0)
                    mesh->TEXCOORD_0_offset[submesh_idx] = view->offset;
                else if (attrib->type == cgltf_attribute_type_texcoord && strcmp(attrib->name, "TEXCOORD_1") == 0)
                    mesh->TEXCOORD_1_offset[submesh_idx] = view->offset;
                else if (attrib->type == cgltf_attribute_type_joints)
                    mesh->JOINTS_0_offset[submesh_idx] = view->offset;
                else if (attrib->type == cgltf_attribute_type_weights)
                    mesh->WEIGHTS_0_offset[submesh_idx] = view->offset;
            }

            mesh->index_offset[submesh_idx] = primitive->indices->buffer_view->offset;
            mesh->index_count[submesh_idx]  = primitive->indices->count;


            //
            // Material data
            //
            auto material_data = &mesh->material_data[submesh_idx];
            if (!primitive->material) continue;
            if (!primitive->material->has_pbr_metallic_roughness) {
                SDL_Log("Only PBR Metal Roughness is supported: %s", _mesh_data->meshes[mesh_idx].primitives->material->name);
                continue;
            }



            auto base_color_texture         = primitive->material->pbr_metallic_roughness.base_color_texture.texture;
            auto metallic_roughness_texture = primitive->material->pbr_metallic_roughness.metallic_roughness_texture.texture;
            auto normal_texture             = primitive->material->normal_texture.texture;
            auto emissive_texture           = primitive->material->emissive_texture.texture;

            if (base_color_texture) {
                LoadTexture(&material_data->base_color_texture_idx, base_color_texture->image->uri, folder_path);
            }
            if (metallic_roughness_texture) {
                LoadTexture(&material_data->metallic_roughness_texture_idx, metallic_roughness_texture->image->uri, folder_path);
            }
            if (normal_texture) {
                LoadTexture(&material_data->metallic_roughness_texture_idx, normal_texture->image->uri, folder_path);
            }
            if (emissive_texture) {
                LoadTexture(&material_data->metallic_roughness_texture_idx, emissive_texture->image->uri, folder_path);
            }

            material_data->base_color_factor.r = primitive->material->pbr_metallic_roughness.base_color_factor[0];
            material_data->base_color_factor.g = primitive->material->pbr_metallic_roughness.base_color_factor[1];
            material_data->base_color_factor.b = primitive->material->pbr_metallic_roughness.base_color_factor[2];
            material_data->base_color_factor.a = primitive->material->pbr_metallic_roughness.base_color_factor[3];

            material_data->metallic_factor  = primitive->material->pbr_metallic_roughness.metallic_factor;
            material_data->roughness_factor = primitive->material->pbr_metallic_roughness.roughness_factor;
        }

        mesh->id = global_instance_id++;
    }



    //
    // Animations
    //
    auto data = _mesh_data; // yayaya
    // if no animations to handle, we just end here
    if (!data->animations_count) return;

    _animations.resize(data->animations_count);
    for (size_t animation_idx = 0; animation_idx < data->animations_count; animation_idx++) {
        Animation *anim = &_animations[animation_idx];

        _animations[animation_idx] = {
            .handle   = &data->animations[animation_idx],
            .name     = data->animations[animation_idx].name,
            .duration = AnimationGetClipDuration(&data->animations[animation_idx]),
        };
        // fixme: delete
        anim->joint_matrices.resize(data->skins->joints_count);

        for (size_t i = 0; i < anim->joint_matrices.size(); i++) {
            anim->joint_matrices[i] = glm::mat4(1);
        }
    }




    ////// Setup Skeleton: flatten **joints array
    assert(data->skins_count == 1);

    auto skin     = data->skins;
    auto joints   = skin->joints;
    auto rig_node = skin->joints[0]->parent;
    _joints.resize(skin->joints_count);

    for (size_t joint_idx = 0; joint_idx < skin->joints_count; joint_idx++) {
        _joints[joint_idx].name = joints[joint_idx]->name; // fixme: animation.joints[i].name will crash if cgltf_data is freed;

        // set inv_bind_matrix
        uint8_t   *data_buffer       = (uint8_t *)skin->inverse_bind_matrices->buffer_view->buffer->data;
        auto       offset            = skin->inverse_bind_matrices->buffer_view->offset;
        glm::mat4 *inv_bind_matrices = ((glm::mat4 *)(data_buffer + offset));

        _joints[joint_idx].inv_bind_matrix = inv_bind_matrices[joint_idx];

        auto current_joint_parent = joints[joint_idx]->parent;

        if (joint_idx == 0 || current_joint_parent == rig_node)
            _joints[joint_idx].parent = -1;
        else {

            for (size_t i = 1; i < skin->joints_count; i++) {
                if (joints[i]->parent == current_joint_parent) {
                    _joints[joint_idx].parent = i - 1;
                    break;
                }
            }
        }
    }

    for (size_t i = 0; i < _animations.size(); i++) {
        _animations[i]._joints = _joints;
    }





    // ////////// todo: figure out if we should actually go this route
    // ////// Process animation samples
    // auto anims_data = data->animations;

    // _animations_v2.resize(data->animations_count);
    // for (size_t anim_idx = 0; anim_idx < data->animations_count; anim_idx++) {
    //     int sample_count = AnimationMaxSampleCount(anims_data);
    //     _animations_v2[anim_idx].samples.resize(skin->joints_count);

    //     // For each Joint
    //     for (size_t joint_idx = 0; joint_idx < skin->joints_count; joint_idx++) {

    //         // For each Channel
    //         for (size_t chan_idx = 0; chan_idx < anims_data->channels_count; chan_idx++) {
    //             cgltf_animation_channel *channel = &anims_data->channels[chan_idx];

    //             // if channel target matches current joint
    //             if (channel->target_node == skin->joints[joint_idx]) {

    //                 cgltf_animation_sampler *sampler = channel->sampler;

    //                 _animations_v2[anim_idx].samples[joint_idx].target_joint = joint_idx;

    //                 auto   input_data_ptr    = (uint8_t *)sampler->input->buffer_view->buffer->data;
    //                 auto   input_data_offset = sampler->input->buffer_view->offset;
    //                 float *timestamps_data   = (float *)(input_data_ptr + input_data_offset);

    //                 auto   output_data_ptr     = (uint8_t *)sampler->output->buffer_view->buffer->data;
    //                 auto   output_data_offset  = sampler->output->buffer_view->offset;
    //                 float *transformation_data = (float *)(output_data_ptr + output_data_offset);

    //                 if (channel->target_path == cgltf_animation_path_type_translation) {
    //                     _animations_v2[anim_idx].samples[joint_idx].translation_channel.timestamps   = timestamps_data;
    //                     _animations_v2[anim_idx].samples[joint_idx].translation_channel.translations = (glm::vec3 *)transformation_data;

    //                 } else if (channel->target_path == cgltf_animation_path_type_rotation) {
    //                     _animations_v2[anim_idx].samples[joint_idx].rotation_channel.timestamps = timestamps_data;
    //                     _animations_v2[anim_idx].samples[joint_idx].rotation_channel.rotations  = (glm::quat *)transformation_data;
    //                 } else if (channel->target_path == cgltf_animation_path_type_scale) {
    //                     _animations_v2[anim_idx].samples[joint_idx].scale_channel.timestamps = timestamps_data;
    //                     _animations_v2[anim_idx].samples[joint_idx].scale_channel.scales     = (glm::vec3 *)transformation_data; // fixme: we might want to scope this to uniform scale only
    //                 }
    //             }
    //         } // End of channels loop for the current joint
    //     } // End of Joints loop
    // }
}

void SkinnedModel::Draw(VkCommandBuffer command_buffer, uint32_t frame_in_flight)
{
    VkBuffer buffers[6];

    // if (_animations.size() > 0)
    {
        buffers[0] = vertex_buffer;
        buffers[1] = vertex_buffer;
        buffers[2] = vertex_buffer;
        buffers[3] = vertex_buffer;
        buffers[4] = vertex_buffer;
        buffers[5] = vertex_buffer;
    }
    // else
    // {
    //     buffers[0] = vertex_buffer;
    //     buffers[1] = vertex_buffer;
    //     buffers[2] = vertex_buffer;
    //     buffers[3] = VK_NULL_HANDLE;
    //     buffers[4] = VK_NULL_HANDLE;
    //     buffers[5] = VK_NULL_HANDLE;
    // }

    for (size_t mesh_idx = 0; mesh_idx < _meshes.size(); mesh_idx++) {

        for (size_t submesh_idx = 0; submesh_idx < _meshes[mesh_idx].index_count.size(); submesh_idx++) {

            VkDeviceSize offsets[6] {};
            // offsets to the start of non-interleaved attributes within same buffer
            // if (_animations.size() > 0)
            {
                /*POSITION  */ offsets[0] = _meshes[mesh_idx].POSITION_offset[submesh_idx];
                /*NORMAL    */ offsets[1] = _meshes[mesh_idx].NORMAL_offset[submesh_idx];
                /*TEXCOORD_0*/ offsets[2] = _meshes[mesh_idx].TEXCOORD_0_offset[submesh_idx];
                /*TEXCOORD_1*/ offsets[3] = _meshes[mesh_idx].TEXCOORD_1_offset[submesh_idx];
                /*JOINTS_0  */ offsets[4] = _meshes[mesh_idx].JOINTS_0_offset[submesh_idx];
                /*WEIGHTS_0 */ offsets[5] = _meshes[mesh_idx].WEIGHTS_0_offset[submesh_idx];
            }
            // else {
            //     /*POSITION  */ offsets[0] = _meshes[mesh_idx].POSITION_offset[submesh_idx];
            //     /*NORMAL    */ offsets[1] = _meshes[mesh_idx].NORMAL_offset[submesh_idx];
            //     /*TEXCOORD_0*/ offsets[2] = _meshes[mesh_idx].TEXCOORD_0_offset[submesh_idx];
            //     /*TEXCOORD_1*/ offsets[3] = 0;
            //     /*JOINTS_0  */ offsets[4] = 0;
            //     /*WEIGHTS_0 */ offsets[5] = 0;
            // }

            ((MaterialData *)mapped_material_data_ptrs[frame_in_flight])[_meshes[mesh_idx].id] = _meshes[mesh_idx].material_data[submesh_idx];

            vkCmdBindVertexBuffers(command_buffer, 0, ARR_COUNT(buffers), buffers, offsets);
            vkCmdBindIndexBuffer(command_buffer, vertex_buffer, _meshes[mesh_idx].index_offset[submesh_idx], VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(command_buffer, _meshes[mesh_idx].index_count[submesh_idx], 1, 0, 0, _meshes[mesh_idx].id);
        }
    }
}

void SkinnedModel::AnimationUpdate(float dt)
{

    if (!_should_play_animation) return;

    if (!_current_animation) {
        SDL_Log("CurrentAnimation not set");
        return;
    }

    auto anim = _current_animation;

    assert(anim->duration > 0);
    anim->isPlaying = true;

    anim->globalTimer += dt;
    float animTime = fmodf(anim->globalTimer, anim->duration);


    static int iterationNb = 0;

    if (iterationNb++ == 0) {
        SDL_Log("playing:[%s] | duration: %fsec", ((cgltf_animation *)anim->handle)->name, anim->duration);
    }

    // For each Joint
    int channel_idx = 0;
    for (size_t joint_idx = 0; joint_idx < anim->_joints.size(); joint_idx++) {

        auto channels   = ((cgltf_animation *)anim->handle)->channels;
        int  currentKey = -1;
        int  nextKey    = -1;

        auto *sampler = channels[channel_idx].sampler;
        for (size_t timestamp_idx = 0; timestamp_idx < sampler->input->count - 1; timestamp_idx++) {
            float sampled_time = readFloatFromAccessor(sampler->input, timestamp_idx + 1);
            float sampled_time_prev;

            if (sampled_time > animTime) {
                currentKey = timestamp_idx;
                nextKey    = currentKey + 1;
                break;
            }
        }


        Transform currentPoseTransform;

        if (channels[channel_idx].target_path == cgltf_animation_path_type_translation) {
            currentPoseTransform.translation = getVec3AtKeyframe(channels[channel_idx].sampler, currentKey);
            ++channel_idx;
        }
        if (channels[channel_idx].target_path == cgltf_animation_path_type_rotation) {
            currentPoseTransform.rotation = getQuatAtKeyframe(channels[channel_idx].sampler, currentKey);
            ++channel_idx;
        }
        if (channels[channel_idx].target_path == cgltf_animation_path_type_scale) {
            currentPoseTransform.scale = getVec3AtKeyframe(channels[channel_idx].sampler, currentKey);
            ++channel_idx;
        }

        auto current_pose_matrix = currentPoseTransform.GetLocalMatrix();
        auto parent_idx          = anim->_joints[joint_idx].parent;

        // comppute global matrix for current joint
        if (parent_idx == -1 && joint_idx > 0)
            anim->_joints[joint_idx].global_joint_matrix = current_pose_matrix;
        else if (joint_idx == 0)
            anim->_joints[0].global_joint_matrix = current_pose_matrix;
        else
            anim->_joints[joint_idx].global_joint_matrix = anim->_joints[parent_idx].global_joint_matrix * current_pose_matrix;

        anim->joint_matrices[joint_idx] = anim->_joints[joint_idx].global_joint_matrix * anim->_joints[joint_idx].inv_bind_matrix;
    } // End of Joints loop
}