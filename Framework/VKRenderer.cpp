
#include "pch.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define VMA_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION

#define FRAMEWORK_EXPORTS
#include "VKRenderer.h"

#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_vulkan.h>


#pragma warning(disable: 6011)

// node(ad) this is used for bindless ObjectData & MaterialDataSSBO shader resources
// which can be indexed for every rendered mesh
constexpr int PROMPT_GPU_SELECTION_AT_STARTUP = 0;
VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_8_BIT;

// VK_PRESENT_MODE_FIFO_KHR; // widest support / VSYNC
// VK_PRESENT_MODE_IMMEDIATE_KHR; // present as fast as possible, high tearing chance
// VK_PRESENT_MODE_MAILBOX_KHR; // present as fast as possible, low tearing chance
VkPresentModeKHR present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

#define SECONDS(value)                  (1000000000 * value)
#define ARR_COUNT(arr)                  (sizeof(arr) / sizeof(arr[0]))


VKDevice gDevice;
VKDeviceContext gDeviceContext;

namespace FMK {


    // this is where resources are cached right now
    std::unordered_map<std::string, void*>    gCgltfData;
    std::unordered_map<std::string, void*>    gSharedMeshes;
    std::unordered_map<std::string, Mesh*>    gSharedMesh;
    std::unordered_map<std::string, Texture*> gTextures;

    static Camera* gActive_camera = NULL;

    void CreateGraphicsPipeline(VkDevice device, VkPipeline* pipeline);

    ////////////////////////////
    // Runtime data






    uint32_t        gFrame_in_flight_idx = 0;
    VkCommandBuffer gGraphics_cmd_buffer_in_flight;



    void InitRenderer(int width, int height)
    {
        gDevice.Create(width, height);
        gDeviceContext.Create();
        CreateGraphicsPipeline(gDevice.device, &gDeviceContext.default_graphics_pipeline);
    }


    //
    // Camera
    //
    void Camera::CameraCreate(glm::vec3 position, float fov, float aspect, float __near, float __far)
    {
        _forward = { 0, 0, -1 };
        _right = {};
        _up = { 0, 1, 0 };
        _yaw = 0;
        _pitch = 0;
        _fov = fov;
        _aspect = 16 / (float)9;
        _position = position;
        _near = __near;
        _far = __far;

        _at = _position + _forward;
        _projection = glm::perspective(glm::radians(_fov), _aspect, _near, _far);
        _view = glm::lookAt(_position, _at, _up);

        mode = FREEFLY;
    }

    void Camera::CameraUpdate(Input* input, float dt, glm::vec3 target)
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
            }
            else
                SDL_SetRelativeMouseMode(SDL_FALSE);

            _forward.x = cosf(glm::radians(_yaw)) * cosf(glm::radians(_pitch));
            _forward.y = sinf(glm::radians(_pitch));
            _forward.z = sinf(glm::radians(_yaw)) * cosf(glm::radians(_pitch));
            _forward = glm::normalize(_forward);
            xrelPrev = (float)xrel;
            yrelPrev = (float)yrel;

            _at = _position + _forward;
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

            static float radius = 6;

            SDL_GetRelativeMouseState(&xrel, &yrel);
            if (input->mouse.right) {
                SDL_SetRelativeMouseMode(SDL_TRUE);

                if (xrelPrev != xrel) _yaw += (float)xrel * _sensitivity;
                if (yrelPrev != yrel && _pitch >= -89.f) _pitch -= (float)yrel * _sensitivity;

                if (_pitch < -75.f) _pitch = -75.f;
                if (_pitch > -5.f) _pitch = -5.f;
            }
            else
                SDL_SetRelativeMouseMode(SDL_FALSE);

            static float camera_height = 1;

            static glm::vec3 offset{};
            offset.x = cosf(glm::radians(_yaw)) * cosf(glm::radians(_pitch)) * radius;
            offset.y = sinf(glm::radians(_pitch)) * radius * -1;
            offset.z = sinf(glm::radians(_yaw)) * cosf(glm::radians(_pitch)) * radius;
            _forward = (offset);
            _right = (glm::cross(_forward, _up));

            xrelPrev = (float)xrel;
            yrelPrev = (float)yrel;

            _position = target + offset;
            _view = glm::lookAt(_position, target + glm::vec3(0, camera_height, 0), _up);
            //_view = glm::mat4(glm::mat3(_view));
        }
        break;

        default:
            break;
        }

        _projection = glm::perspective(_fov, _aspect, _near, _far);
    }





    //
    // Texture
    //
    void LoadAndCacheTexture(int32_t* texture_idx, const char* texture_uri, const char* folder_path)
    {
        std::string full_path;
        full_path.append(folder_path);
        full_path.append(texture_uri);

        Texture* t = (Texture*)malloc(sizeof(Texture));
        assert(t != NULL);

        t->Create(full_path.c_str());
        t->name = _strdup(texture_uri);

        gTextures.insert({ std::string(texture_uri), t });

        *texture_idx = t->descriptor_array_idx;
    }












    //
    // Skaletal Animation handling
    //

    float AnimationGetClipDuration(cgltf_animation* animationClip)
    {
        float duration = 0;
        float lastMax = 0;

        for (size_t i = 0; i < animationClip->samplers_count; i++) {
            cgltf_accessor* input = animationClip->samplers[i].input;

            for (size_t j = 0; j < ARR_COUNT(input->max); j++) {
                if (input->has_max && input->max[j] > lastMax)
                    lastMax = input->max[j];
            }
            duration = lastMax;
        }
        return duration;
    }

    int AnimationMaxSampleCount(cgltf_animation* animationClip)
    {
        int last_max = 0;

        for (size_t i = 0; i < animationClip->samplers_count; i++) {
            auto* input = animationClip->samplers[i].input;

            if (input->count > last_max) {
                last_max = (int)input->count;
            }
        }
        return last_max;
    }

    float readFloatFromAccessor(cgltf_accessor* accessor, cgltf_size idx)
    {
        float value;
        // SDL_Log("idx:[%d]", idx);
        if (cgltf_accessor_read_float(accessor, idx, &value, accessor->count))
            return value;
        else
            return 0;
    }

    glm::vec3 getVec3AtKeyframe(cgltf_animation_sampler* sampler, int keyframe)
    {
        // todo: check for failure?
        void* values = (void*)((uint8_t*)sampler->output->buffer_view->buffer->data + sampler->output->buffer_view->offset);
        return ((glm::vec3*)values)[keyframe];
    }

    glm::quat getQuatAtKeyframe(cgltf_animation_sampler* sampler, int keyframe)
    {
        // todo: check for failure?
        void* values = (void*)((uint8_t*)sampler->output->buffer_view->buffer->data + sampler->output->buffer_view->offset);
        return ((glm::quat*)values)[keyframe];
    }

    void AnimationUpdate(Animation* anim, float dt)
    {

        /*if (!model->_should_play_animation) return;
        if (!model->_current_animation) {
            SDL_Log("CurrentAnimation not set");
            return;
        }
        auto anim = model->_current_animation;*/

        assert(anim->handle);
        assert(anim->duration > 0);
        anim->isPlaying = true;

        anim->globalTimer += dt;
        float animTime = fmodf(anim->globalTimer, anim->duration);


        static int iterationNb = 0;

        if (iterationNb++ == 0) {
            SDL_Log("playing:[%s] | duration: %fsec", ((cgltf_animation*)anim->handle)->name, anim->duration);
        }

        // For each Joint
        int channel_idx = 0;
        for (size_t joint_idx = 0; joint_idx < anim->_joints.size(); joint_idx++) {

            auto channels = ((cgltf_animation*)anim->handle)->channels;
            int  currentKey = -1;
            int  nextKey = -1;

            auto* sampler = channels[channel_idx].sampler;
            for (int timestamp_idx = 0; timestamp_idx < sampler->input->count - 1; timestamp_idx++) {
                float sampled_time = readFloatFromAccessor(sampler->input, static_cast<cgltf_size>(timestamp_idx) + 1);
                //float sampled_time_prev;

                if (sampled_time > animTime) {
                    currentKey = timestamp_idx;
                    nextKey = currentKey + 1;
                    break;
                }
            }

            float currentFrameTime = animTime - readFloatFromAccessor(sampler->input, currentKey);
            float frameDuration = readFloatFromAccessor(sampler->input, nextKey) - readFloatFromAccessor(sampler->input, currentKey);
            float t = currentFrameTime / frameDuration;

            Transform currentPoseTransform;

            if (channels[channel_idx].target_path == cgltf_animation_path_type_translation) {
                auto currentVec3 = getVec3AtKeyframe(channels[channel_idx].sampler, currentKey);
                auto nextVec3 = getVec3AtKeyframe(channels[channel_idx].sampler, nextKey);
                currentPoseTransform.translation = glm::mix(currentVec3, nextVec3, t);

                ++channel_idx;
            }
            if (channels[channel_idx].target_path == cgltf_animation_path_type_rotation) {
                auto currentQuat = getQuatAtKeyframe(channels[channel_idx].sampler, currentKey);
                auto nextQuat = getQuatAtKeyframe(channels[channel_idx].sampler, nextKey);
                currentPoseTransform.rotation = glm::slerp(currentQuat, nextQuat, t);

                ++channel_idx;
            }
            if (channels[channel_idx].target_path == cgltf_animation_path_type_scale) {
                auto currentVec3 = getVec3AtKeyframe(channels[channel_idx].sampler, currentKey);
                auto nextVec3 = getVec3AtKeyframe(channels[channel_idx].sampler, nextKey);
                currentPoseTransform.scale = glm::mix(currentVec3, nextVec3, t);
                ++channel_idx;
            }

            auto current_pose_matrix = currentPoseTransform.GetLocalMatrix();
            auto parent_idx = anim->_joints[joint_idx].parent;

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









    //
    // Utils
    //
    bool LoadCgltfData(const char* path, cgltf_data** data)
    {
        cgltf_options options = {};
        cgltf_result  result = cgltf_parse_file(&options, path, &(*data));

        if (cgltf_parse_file(&options, path, &(*data)) == cgltf_result_success) {
            if (cgltf_load_buffers(&options, (*data), path) == cgltf_result_success) {
                if ((result = cgltf_validate((*data))) != cgltf_result_success) {
                    SDL_Log("cgltf validation error");
                    return false;
                }
                result = cgltf_load_buffers(&options, (*data), path);
            }
        }
        else {
            SDL_Log("Failed to load [%s]", path);
            return false;
        }

        return true;
    }

    // Callee allocates dst
    static errno_t GetPathFolder(char** dst, const char* src, uint32_t srcLength)
    {
        assert(src);
        assert(srcLength > 0);

        size_t size = std::string(src).find_last_of('/');
        *dst = (char*)malloc(sizeof(char) * size + 1);
        return strncpy_s(*(char**)dst, size + 2, src, size + 1);
    }




    // not to be messed with!
    // it is automatically incremented and assigned as a "unique" id
    // to each submesh of a unique mesh that is loaded (we cache meshes)
    // so that the corresponding material data can be retieves in shaders
    static uint32_t global_instance_id = 0;

    void Mesh::Create(const char* mesh_filepath)
    {
        if (gCgltfData.contains(mesh_filepath)) {
            _mesh_data = (cgltf_data*)gCgltfData[mesh_filepath];
            SDL_Log("Data is already cached : %s", mesh_filepath);

        }
        else {
            cgltf_data* data;
            LoadCgltfData(mesh_filepath, &data);
            SDL_Log("Loading : %s", mesh_filepath);

            gCgltfData.insert({ std::string(mesh_filepath), (void*)data });
            _mesh_data = data;
        }

        if (gSharedMesh.contains(mesh_filepath)) {
            SDL_Log("Mesh is already cached : %s", mesh_filepath);
            return;
        }


        size_t vertex_buffer_size = _mesh_data->buffers->size;

        VkBuffer staging_buffer;
        VmaAllocation staging_buffer_allocation{};
        {
            VkBufferCreateInfo buffer_ci{};
            buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size = vertex_buffer_size;
            buffer_ci.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo vma_allocation_ci{};
            vma_allocation_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            vma_allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            // TODO: upload to GPU exclusive memory instead of a mapped ptr!!!

            vmaCreateBuffer(gDeviceContext.allocator, &buffer_ci, &vma_allocation_ci, &staging_buffer, &staging_buffer_allocation, NULL);
            void* mapped_vertex_buffer_ptr;
            vmaMapMemory(gDeviceContext.allocator, staging_buffer_allocation, &mapped_vertex_buffer_ptr);

            // for now, as a quick first pass, we load and keep the entire gltf .bin data
            // in shared GPU memory, bad bad
            // future improvements should upload geometry data to GPU exclusive vertex buffer (verts & indices in the same buffer)
            // might call it geometry_buffer for lack of a better name.
            // I haven't done the math to see if we should pre-cache ALL geometry data into a single buffer
            memcpy(mapped_vertex_buffer_ptr, _mesh_data->buffers->data, vertex_buffer_size);
            vmaUnmapMemory(gDeviceContext.allocator, staging_buffer_allocation);
        }



        // create & allocate vertex buffer
        {

            VkBufferCreateInfo buffer_ci{};
            buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size = vertex_buffer_size;
            buffer_ci.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo vma_allocation_ci{};
            vma_allocation_ci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            vma_allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            // TODO: upload to GPU exclusive memory instead of a mapped ptr!!!

            vmaCreateBuffer(gDeviceContext.allocator, &buffer_ci, &vma_allocation_ci, &vertex_buffer, &vertex_buffer_allocation, NULL);
        }


        VkBufferCopy2 copy_regions{};
        copy_regions.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        copy_regions.pNext = NULL;
        copy_regions.srcOffset = 0;
        copy_regions.dstOffset = 0;
        copy_regions.size = vertex_buffer_size;

        VkCopyBufferInfo2 copy_buffer_info{};
        copy_buffer_info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        copy_buffer_info.pNext = NULL;
        copy_buffer_info.srcBuffer = staging_buffer;
        copy_buffer_info.dstBuffer = vertex_buffer;
        copy_buffer_info.regionCount = 1;
        copy_buffer_info.pRegions = &copy_regions;



        VkCommandBufferBeginInfo cmd_buffer_begin_info = {};
        cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VKCHECK(vkBeginCommandBuffer(gDeviceContext.frames[0].graphics_command_buffer, &cmd_buffer_begin_info));
        vkCmdCopyBuffer2(gDeviceContext.frames[0].graphics_command_buffer, &copy_buffer_info);
        VKCHECK(vkEndCommandBuffer(gDeviceContext.frames[0].graphics_command_buffer));

        // Submit command buffer and copy data from staging buffer to a vertex buffer
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &gDeviceContext.frames[0].graphics_command_buffer;
        VKCHECK(vkQueueSubmit(gDevice.queues.graphics, 1, &submit_info, VK_NULL_HANDLE));
        VKCHECK(vkDeviceWaitIdle(gDevice.device));








        _meshes.resize(_mesh_data->meshes_count);

        for (size_t mesh_idx = 0; mesh_idx < _mesh_data->meshes_count; mesh_idx++) {
            auto mesh = &_meshes[mesh_idx];
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
                cgltf_primitive* primitive = &_mesh_data->meshes[mesh_idx].primitives[submesh_idx];

                for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                    cgltf_attribute* attrib = &primitive->attributes[attrib_idx];
                    cgltf_buffer_view* view = attrib->data->buffer_view;

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
                mesh->index_count[submesh_idx] = (uint32_t)primitive->indices->count;


                //
                // Material data
                //
                auto material_data = &mesh->material_data[submesh_idx];

                bool material_data_is_null = !primitive->material;
                if (material_data_is_null) continue;

                bool no_mettalic_worklow = !primitive->material->has_pbr_metallic_roughness;
                if (no_mettalic_worklow) {
                    SDL_Log("Only PBR Metal Roughness is supported: %s", _mesh_data->meshes[mesh_idx].primitives->material->name);
                    continue;
                }


                auto base_color_texture = primitive->material->pbr_metallic_roughness.base_color_texture.texture;
                auto metallic_roughness_texture = primitive->material->pbr_metallic_roughness.metallic_roughness_texture.texture;
                auto normal_texture = primitive->material->normal_texture.texture;
                auto emissive_texture = primitive->material->emissive_texture.texture;

                char* textures_folder_path;
                GetPathFolder(&textures_folder_path, mesh_filepath, (uint32_t)strlen(mesh_filepath));

                // fixme cache texture
                if (base_color_texture) {
                    LoadAndCacheTexture(&material_data->base_color_texture_idx, base_color_texture->image->uri, textures_folder_path);
                }
                if (metallic_roughness_texture) {
                    LoadAndCacheTexture(&material_data->metallic_roughness_texture_idx, metallic_roughness_texture->image->uri, textures_folder_path);
                }
                if (normal_texture) {
                    LoadAndCacheTexture(&material_data->normal_texture_idx, normal_texture->image->uri, textures_folder_path);
                }
                if (emissive_texture) {
                    LoadAndCacheTexture(&material_data->emissive_texture_idx, emissive_texture->image->uri, textures_folder_path);
                }

                material_data->base_color_factor.r = primitive->material->pbr_metallic_roughness.base_color_factor[0];
                material_data->base_color_factor.g = primitive->material->pbr_metallic_roughness.base_color_factor[1];
                material_data->base_color_factor.b = primitive->material->pbr_metallic_roughness.base_color_factor[2];
                material_data->base_color_factor.a = primitive->material->pbr_metallic_roughness.base_color_factor[3];

                material_data->metallic_factor = primitive->material->pbr_metallic_roughness.metallic_factor;
                material_data->roughness_factor = primitive->material->pbr_metallic_roughness.roughness_factor;
            }

            mesh->instance_id = global_instance_id++;
        }

        //fixme cache mesh
        //gSharedMesh.insert({ , this });


        //
        // Animations
        //
        auto data = _mesh_data; // yayaya
        // if no animations to handle, we just end here
        if (!data->animations_count) return;

        _animations.resize(data->animations_count);
        for (size_t animation_idx = 0; animation_idx < data->animations_count; animation_idx++) {
            Animation* anim = &_animations[animation_idx];

            /*_animations[animation_idx]; = {
                .handle = &data->animations[animation_idx],
                .name = data->animations[animation_idx].name,
                .duration = AnimationGetClipDuration(&data->animations[animation_idx]),
            };*/
            // fixme: delete

            anim->handle = &data->animations[animation_idx];
            anim->name = data->animations[animation_idx].name;
            anim->duration = AnimationGetClipDuration(&data->animations[animation_idx]);


            for (size_t i = 0; i < MAX_JOINTS; i++) {
                anim->joint_matrices[i] = glm::mat4(1);
            }
        }




        ////// Setup Skeleton: flatten **joints array
        assert(data->skins_count == 1);

        auto skin = data->skins;
        auto joints = skin->joints;
        auto rig_node = skin->joints[0]->parent;
        _joints.resize(skin->joints_count);

        for (size_t joint_idx = 0; joint_idx < skin->joints_count; joint_idx++) {
            _joints[joint_idx].name = joints[joint_idx]->name; // fixme: animation.joints[i].name will crash if cgltf_data is freed;

            // set inv_bind_matrix
            uint8_t* data_buffer = (uint8_t*)skin->inverse_bind_matrices->buffer_view->buffer->data;
            auto       offset = skin->inverse_bind_matrices->buffer_view->offset;
            glm::mat4* inv_bind_matrices = ((glm::mat4*)(data_buffer + offset));

            _joints[joint_idx].inv_bind_matrix = inv_bind_matrices[joint_idx];

            auto current_joint_parent = joints[joint_idx]->parent;

            if (joint_idx == 0 || current_joint_parent == rig_node)
                _joints[joint_idx].parent = -1;
            else {

                for (int8_t i = 1; i < skin->joints_count; i++) {
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

    void Draw(const Mesh* model)
    {
        VkBuffer buffers[6]{};

        // if (_animations.size() > 0)
        {
            buffers[0] = model->vertex_buffer;
            buffers[1] = model->vertex_buffer;
            buffers[2] = model->vertex_buffer;
            buffers[3] = model->vertex_buffer;
            buffers[4] = model->vertex_buffer;
            buffers[5] = model->vertex_buffer;
        }

        for (size_t mesh_idx = 0; mesh_idx < model->_meshes.size(); mesh_idx++) {

            for (size_t submesh_idx = 0; submesh_idx < model->_meshes[mesh_idx].index_count.size(); submesh_idx++) {

                VkDeviceSize offsets[6]{};
                /*POSITION  */ offsets[0] = model->_meshes[mesh_idx].POSITION_offset[submesh_idx];
                /*NORMAL    */ offsets[1] = model->_meshes[mesh_idx].NORMAL_offset[submesh_idx];
                /*TEXCOORD_0*/ offsets[2] = model->_meshes[mesh_idx].TEXCOORD_0_offset[submesh_idx];
                /*TEXCOORD_1*/ offsets[3] = model->_meshes[mesh_idx].TEXCOORD_1_offset[submesh_idx];
                /*JOINTS_0  */ offsets[4] = model->_meshes[mesh_idx].JOINTS_0_offset[submesh_idx];
                /*WEIGHTS_0 */ offsets[5] = model->_meshes[mesh_idx].WEIGHTS_0_offset[submesh_idx];


                // fixme provide SetMaterialData() ?
                auto instance_id = model->_meshes[mesh_idx].instance_id;
                ((MaterialData*)gDeviceContext.mapped_material_data_ptrs[gFrame_in_flight_idx])[instance_id] = model->_meshes[mesh_idx].material_data[submesh_idx];

                vkCmdBindVertexBuffers(gGraphics_cmd_buffer_in_flight, 0, ARR_COUNT(buffers), buffers, offsets);
                vkCmdBindIndexBuffer(gGraphics_cmd_buffer_in_flight, model->vertex_buffer, model->_meshes[mesh_idx].index_offset[submesh_idx], VK_INDEX_TYPE_UINT16);
                vkCmdDrawIndexed(gGraphics_cmd_buffer_in_flight, model->_meshes[mesh_idx].index_count[submesh_idx], 1, 0, 0, instance_id);
            }
        }
    }













    //void DrawIndexed(const VkBuffer* vertex_buffers, uint32_t buffer_count, const VkDeviceSize* vertex_offsets, const VkBuffer index_buffer, uint32_t index_offset, uint32_t index_count, const MaterialData* material_data, uint32_t mesh_id)
    //{
    //	((MaterialData*)mapped_material_data_ptrs[gFrame_in_flight_idx])[mesh_id] = *material_data;
    //
    //	vkCmdBindVertexBuffers(gGraphics_cmd_buffer_in_flight, 0, buffer_count, vertex_buffers, vertex_offsets);
    //	vkCmdBindIndexBuffer(gGraphics_cmd_buffer_in_flight, index_buffer, index_offset, VK_INDEX_TYPE_UINT16);
    //	vkCmdDrawIndexed(gGraphics_cmd_buffer_in_flight, index_count, 1, 0, 0, mesh_id);
    //}






    void BeginRendering()
    {
        VkResult result;

        auto frame_in_flight = &gDeviceContext.frames[gFrame_in_flight_idx];
        gGraphics_cmd_buffer_in_flight = frame_in_flight->graphics_command_buffer;

        VKCHECK(vkWaitForFences(gDevice.device, 1, &frame_in_flight->render_fence, VK_TRUE, SECONDS(1)));
        VKCHECK(vkResetFences(gDevice.device, 1, &frame_in_flight->render_fence));
        VKCHECK(result = vkAcquireNextImageKHR(gDevice.device, gDeviceContext.swapchain._handle, SECONDS(1), 0, frame_in_flight->render_fence, &frame_in_flight->image_idx));
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            gDeviceContext.swapchain.Create(gDevice.surface, gDeviceContext.swapchain._handle);
        }

        VKCHECK(vkResetCommandBuffer(gGraphics_cmd_buffer_in_flight, 0));

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKCHECK(vkBeginCommandBuffer(gGraphics_cmd_buffer_in_flight, &begin_info));





        //
        // Swapchain image layout transition for Rendering
        //
        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.levelCount = 1;
        subresource_range.layerCount = 1;

        TransitionImageLayout(gGraphics_cmd_buffer_in_flight, gDeviceContext.swapchain._images[frame_in_flight->image_idx],
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, subresource_range, false);

        TransitionImageLayout(gGraphics_cmd_buffer_in_flight, gDeviceContext.swapchain._msaa_image.handle,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, subresource_range, false);


        VkClearValue clear_values[2] = {};
        clear_values[0].color = { .13f, .13f, .13f, 1.f };

        VkRenderingAttachmentInfo color_attachment_info[2] = {};

        color_attachment_info[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment_info[0].imageView = gDeviceContext.swapchain._msaa_image_view;
        color_attachment_info[0].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        color_attachment_info[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_info[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_info[0].clearValue = clear_values[0];

        color_attachment_info[0].resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        color_attachment_info[0].resolveImageView = gDeviceContext.swapchain._image_views[frame_in_flight->image_idx];
        color_attachment_info[0].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;

        /*color_attachment_info[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment_info[1].imageView = gDeviceContext.swapchain._msaa_image_view;
        color_attachment_info[1].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        color_attachment_info[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_info[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_info[1].clearValue = clear_values[0];
*/

        clear_values[1].depthStencil.depth = { 1.f };

        VkRenderingAttachmentInfo depth_attachment_info = {};
        depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment_info.imageView = gDeviceContext.swapchain._depth_image_view;
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment_info.clearValue = clear_values[1];

        VkRenderingInfo rendering_info = {};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.extent = { (uint32_t)gDevice.width, (uint32_t)gDevice.height };
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment_info[0];
        rendering_info.pDepthAttachment = &depth_attachment_info;
        rendering_info.pStencilAttachment; // todo
        vkCmdBeginRendering(gGraphics_cmd_buffer_in_flight, &rendering_info);





        ////////////////////////////////////////////
        // Rendering
        //

        vkCmdBindPipeline(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDeviceContext.default_graphics_pipeline);

        vkCmdBindDescriptorSets(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDeviceContext.default_graphics_pipeline_layout, 0, 1, &gDeviceContext.view_projection_sets[gFrame_in_flight_idx], 0, NULL);
        vkCmdBindDescriptorSets(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDeviceContext.default_graphics_pipeline_layout, 1, 1, &gDeviceContext.draw_data_sets[gFrame_in_flight_idx], 0, NULL);
        vkCmdBindDescriptorSets(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDeviceContext.default_graphics_pipeline_layout, 2, 1, &gDeviceContext.material_data_sets[gFrame_in_flight_idx], 0, NULL);
        vkCmdBindDescriptorSets(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDeviceContext.default_graphics_pipeline_layout, 3, 1, &gDeviceContext.bindless_textures_set, 0, NULL);

        auto global_uniforms = ((GlobalUniforms*)gDeviceContext.mapped_view_proj_ptrs[gFrame_in_flight_idx]);
        global_uniforms->projection = gActive_camera->_projection;
        global_uniforms->view = gActive_camera->_view;
        global_uniforms->projection[1][1] *= -1;
        //global_uniforms->camera_position = { global_uniforms->view[3][0], global_uniforms->view[3][1], global_uniforms->view[3][2] };
        global_uniforms->camera_position = { gActive_camera->_position.x, gActive_camera->_position.y, gActive_camera->_position.z };
        //SDL_Log("view: (%f, %f, %f)\n", gActive_camera->_position.x, gActive_camera->_position.y, gActive_camera->_position.z);

        VkViewport viewport{};
        viewport.minDepth = 0;
        viewport.maxDepth = 1;
        viewport.width = (float)gDevice.width;
        viewport.height = (float)gDevice.height;

        VkRect2D scissor{};
        scissor.extent.width = gDevice.width;
        scissor.extent.height = gDevice.height;

        vkCmdSetViewport(gGraphics_cmd_buffer_in_flight, 0, 1, &viewport);
        vkCmdSetScissor(gGraphics_cmd_buffer_in_flight, 0, 1, &scissor);
    }


    //////////////////////////////////////////////
    void EndRendering() {
        vkCmdEndRendering(gGraphics_cmd_buffer_in_flight);

        //
        // Swapchain image layout transition for presenting
        //
        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.levelCount = 1;
        subresource_range.layerCount = 1;

        TransitionImageLayout(gGraphics_cmd_buffer_in_flight, gDeviceContext.swapchain._images[gDeviceContext.frames[gFrame_in_flight_idx].image_idx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresource_range, false);
        TransitionImageLayout(gGraphics_cmd_buffer_in_flight, gDeviceContext.swapchain._msaa_image.handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresource_range, false);

        VKCHECK(vkEndCommandBuffer(gGraphics_cmd_buffer_in_flight));



        //
        // Command Buffer submission
        //
        {
            VkCommandBufferSubmitInfo command_buffer_submit_info = {};
            command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_info.pNext = NULL;
            command_buffer_submit_info.commandBuffer = gGraphics_cmd_buffer_in_flight;
            command_buffer_submit_info.deviceMask;

            VkSemaphoreSubmitInfo wait_semephore_info = {};
            wait_semephore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            wait_semephore_info.semaphore = gDeviceContext.frames[gFrame_in_flight_idx].present_semaphore;
            wait_semephore_info.value;
            wait_semephore_info.stageMask = VK_PIPELINE_STAGE_NONE;
            wait_semephore_info.deviceIndex;

            VkSemaphoreSubmitInfo signal_semephore_info = {};
            signal_semephore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signal_semephore_info.semaphore = gDeviceContext.frames[gFrame_in_flight_idx].render_semaphore;
            signal_semephore_info.value;
            signal_semephore_info.stageMask = VK_PIPELINE_STAGE_NONE;
            signal_semephore_info.deviceIndex;

            VkSubmitInfo2 submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submit_info.flags;
            submit_info.waitSemaphoreInfoCount = 0;
            // submit_info.pWaitSemaphoreInfos      = &wait_semephore_info; // wait for image to be presented before rendering again
            submit_info.commandBufferInfoCount = 1;
            submit_info.pCommandBufferInfos = &command_buffer_submit_info;
            submit_info.signalSemaphoreInfoCount = 1;
            submit_info.pSignalSemaphoreInfos = &signal_semephore_info;

            VKCHECK(vkWaitForFences(gDevice.device, 1, &gDeviceContext.frames[gFrame_in_flight_idx].render_fence, VK_TRUE, SECONDS(1)));
            VKCHECK(vkResetFences(gDevice.device, 1, &gDeviceContext.frames[gFrame_in_flight_idx].render_fence));

            VKCHECK(vkQueueSubmit2(gDevice.queues.graphics, 1, &submit_info, gDeviceContext.frames[gFrame_in_flight_idx].render_fence));
        }


        {
            //
            // Presenting
            //
            VkPresentInfoKHR present_info = {};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &gDeviceContext.frames[gFrame_in_flight_idx].render_semaphore; // the semaphore to wait upon before presenting
            present_info.swapchainCount = 1;
            present_info.pSwapchains = &gDeviceContext.swapchain._handle;
            present_info.pImageIndices = &gDeviceContext.frames[gFrame_in_flight_idx].image_idx;
            present_info.pResults;

            VkResult result;
            VKCHECK(result = vkQueuePresentKHR(gDevice.queues.present, &present_info));
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                gDeviceContext.swapchain.Create(gDevice.surface, gDeviceContext.swapchain._handle);
            }
        }

        ++gFrame_in_flight_idx;
        gFrame_in_flight_idx = gFrame_in_flight_idx % gDeviceContext.swapchain._image_count;
    }




    bool CreateShaderModule(VkDevice device, const char* filepath, VkShaderModule* out_ShaderModule)
    {

        FILE* f = fopen(filepath, "rb");
        if (!f) {
            SDL_Log("Could not find path to %s : %s", filepath, strerror(errno));
            return NULL;
        }

        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        uint32_t* buffer = (uint32_t*)malloc(fsize);
        assert(buffer);
        fread(buffer, fsize, 1, f);
        fclose(f);

        // create a new shader module, using the buffer we loaded
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.codeSize = fsize;
        createInfo.pCode = buffer;

        VkShaderModule shaderModule;

        auto result = vkCreateShaderModule(device, &createInfo, NULL, &shaderModule);
        if (result != VK_SUCCESS) {
            VKCHECK(result);
            return false;
        }

        *out_ShaderModule = shaderModule;

        return true;
    }



    //
    // Graphics Pipeline
    //
    void CreateGraphicsPipeline(VkDevice device, VkPipeline* pipeline)
    {
        // [x]write shaders
        // [x]compile to spv
        // [x]load spv
        // [x]create modules
        // [x]build pipeline

        VkShaderModule vertex_shader_module{};
        VkShaderModule fragment_shader_module{};
        CreateShaderModule(device, "shaders/standard.vert.spv", &vertex_shader_module);
        CreateShaderModule(device, "shaders/standard.frag.spv", &fragment_shader_module);

        //
        // Pipeline
        //
        VkPipelineShaderStageCreateInfo shader_stage_cis[2]{};
        shader_stage_cis[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_cis[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stage_cis[0].module = vertex_shader_module;
        shader_stage_cis[0].pName = "main";

        shader_stage_cis[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_cis[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stage_cis[1].module = fragment_shader_module;
        shader_stage_cis[1].pName = "main";

        //
        // Raster state
        //
        VkPipelineRasterizationStateCreateInfo raster_state_ci{};
        raster_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster_state_ci.depthClampEnable;
        raster_state_ci.rasterizerDiscardEnable = VK_FALSE;
        raster_state_ci.polygonMode = VK_POLYGON_MODE_FILL;
        raster_state_ci.cullMode = VK_CULL_MODE_BACK_BIT;
        raster_state_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster_state_ci.depthBiasEnable;
        raster_state_ci.depthBiasConstantFactor;
        raster_state_ci.depthBiasClamp;
        raster_state_ci.depthBiasSlopeFactor;
        raster_state_ci.lineWidth = 1.f;


        //
        // Vertex input state - Non-interleaved, so we must bind each vertex attribute to a different slot
        //
        const int                       BINDING_COUNT = 6;
        VkVertexInputBindingDescription vertex_binding_descriptions[BINDING_COUNT]{ 0 };
        vertex_binding_descriptions[0].binding = 0; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[0].stride = sizeof(float) * 3;
        vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[1].binding = 1; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[1].stride = sizeof(float) * 3;
        vertex_binding_descriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[2].binding = 2; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[2].stride = sizeof(float) * 2;
        vertex_binding_descriptions[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[3].binding = 3; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[3].stride = sizeof(float) * 2;
        vertex_binding_descriptions[3].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[4].binding = 4; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[4].stride = sizeof(uint8_t) * 4;
        vertex_binding_descriptions[4].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[5].binding = 5; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[5].stride = sizeof(float) * 4;
        vertex_binding_descriptions[5].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


        const int                         ATTRIB_COUNT = 6;
        VkVertexInputAttributeDescription vertex_attrib_descriptions[ATTRIB_COUNT]{ 0 };
        vertex_attrib_descriptions[0].location = 0;
        vertex_attrib_descriptions[0].binding = 0;
        vertex_attrib_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_attrib_descriptions[0].offset = 0; // 0: tightly packed

        vertex_attrib_descriptions[1].location = 1;
        vertex_attrib_descriptions[1].binding = 1;
        vertex_attrib_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_attrib_descriptions[1].offset = 0;

        vertex_attrib_descriptions[2].location = 2;
        vertex_attrib_descriptions[2].binding = 2;
        vertex_attrib_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        vertex_attrib_descriptions[2].offset = 0;

        vertex_attrib_descriptions[3].location = 3;
        vertex_attrib_descriptions[3].binding = 3;
        vertex_attrib_descriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        vertex_attrib_descriptions[3].offset = 0;

        vertex_attrib_descriptions[4].location = 4;
        vertex_attrib_descriptions[4].binding = 4;
        vertex_attrib_descriptions[4].format = VK_FORMAT_R8G8B8A8_UNORM;
        vertex_attrib_descriptions[4].offset = 0;

        vertex_attrib_descriptions[5].location = 5;
        vertex_attrib_descriptions[5].binding = 5;
        vertex_attrib_descriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        vertex_attrib_descriptions[5].offset = 0;

        VkPipelineVertexInputStateCreateInfo vertex_input_state_ci{};
        vertex_input_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_ci.vertexBindingDescriptionCount = ARR_COUNT(vertex_binding_descriptions);
        vertex_input_state_ci.pVertexBindingDescriptions = vertex_binding_descriptions;
        vertex_input_state_ci.vertexAttributeDescriptionCount = ARR_COUNT(vertex_attrib_descriptions);
        vertex_input_state_ci.pVertexAttributeDescriptions = vertex_attrib_descriptions;

        //
        // Input assembly state
        //
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_ci{};
        input_assembly_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_ci.primitiveRestartEnable;

        //
        // Dynamic states
        //

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
            VK_DYNAMIC_STATE_CULL_MODE,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_ci{};
        dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_ci.dynamicStateCount = ARR_COUNT(dynamic_states);
        dynamic_state_ci.pDynamicStates = dynamic_states;

        //
        // Viewport state
        //
        VkPipelineViewportStateCreateInfo viewport_state_ci{};
        viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_ci.viewportCount = 1;
        viewport_state_ci.pViewports = NULL;
        viewport_state_ci.scissorCount = 1;
        viewport_state_ci.pScissors = NULL;




        //
        // Depth/Stencil state
        //
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_ci{};
        depth_stencil_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_ci.depthTestEnable = VK_TRUE; // tests wether fragment sould be discarded
        depth_stencil_state_ci.depthWriteEnable = VK_TRUE; // should depth test result be written to depth buffer?
        depth_stencil_state_ci.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // keeps fragments that are closer (lower depth)

        // Optional: what is stencil for[check]?
        // depth_stencil_state_ci.stencilTestEnable;
        // depth_stencil_state_ci.front;
        // depth_stencil_state_ci.back;

        // Optional: allows to specify bounds
        // depth_stencil_state_ci.depthBoundsTestEnable;
        // depth_stencil_state_ci.minDepthBounds;
        // depth_stencil_state_ci.maxDepthBounds;




        //
        // Color blend state
        //
        VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
        color_blend_attachment_state.blendEnable = VK_TRUE;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state_ci{};
        color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_ci.logicOpEnable = VK_FALSE;
        color_blend_state_ci.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_ci.attachmentCount = 1;
        color_blend_state_ci.pAttachments = &color_blend_attachment_state;
        color_blend_state_ci.blendConstants[0] = 1.f;
        color_blend_state_ci.blendConstants[1] = 1.f;
        color_blend_state_ci.blendConstants[2] = 1.f;
        color_blend_state_ci.blendConstants[3] = 1.f;



        //
        // Push Contstants
        //
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(PushConstants);


        //
        // Pipeline layout
        //

        std::vector<VkDescriptorSetLayout> set_layouts{
            gDeviceContext.view_projection_set_layout,
            gDeviceContext.draw_data_set_layout,
            gDeviceContext.material_data_set_layout,
            gDeviceContext.bindless_textures_set_layout
        };


        VkPipelineLayoutCreateInfo layout_ci{};
        layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_ci.setLayoutCount = (uint32_t)set_layouts.size();
        layout_ci.pSetLayouts = &set_layouts[0];
        layout_ci.pushConstantRangeCount = 1;
        layout_ci.pPushConstantRanges = &push_constant_range;

        vkCreatePipelineLayout(device, &layout_ci, NULL, &gDeviceContext.default_graphics_pipeline_layout);


        //
        // Multisample state
        //
        VkPipelineMultisampleStateCreateInfo multisample_state_ci{};
        multisample_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state_ci.rasterizationSamples = sample_count; // fixme: hardoded
        multisample_state_ci.sampleShadingEnable = VK_FALSE;
        multisample_state_ci.minSampleShading;
        multisample_state_ci.pSampleMask;
        multisample_state_ci.alphaToCoverageEnable;
        multisample_state_ci.alphaToOneEnable;


        //
        // Pipeline state
        //
        VkPipelineRenderingCreateInfo rendering_ci{};
        rendering_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_ci.colorAttachmentCount = 1;
        rendering_ci.pColorAttachmentFormats = &gDeviceContext.swapchain._format;
        rendering_ci.depthAttachmentFormat = VK_FORMAT_D16_UNORM;

        VkGraphicsPipelineCreateInfo pipeline_ci{};
        pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_ci.pNext = &rendering_ci;
        pipeline_ci.stageCount = ARR_COUNT(shader_stage_cis);
        pipeline_ci.pStages = shader_stage_cis;
        pipeline_ci.pVertexInputState = &vertex_input_state_ci;
        pipeline_ci.pInputAssemblyState = &input_assembly_state_ci;
        pipeline_ci.pTessellationState;
        pipeline_ci.pViewportState = &viewport_state_ci;
        pipeline_ci.pRasterizationState = &raster_state_ci;
        pipeline_ci.pMultisampleState = &multisample_state_ci;
        pipeline_ci.pDepthStencilState = &depth_stencil_state_ci;
        pipeline_ci.pColorBlendState = &color_blend_state_ci;
        pipeline_ci.pDynamicState = &dynamic_state_ci;
        pipeline_ci.layout = gDeviceContext.default_graphics_pipeline_layout;
        pipeline_ci.renderPass;
        pipeline_ci.subpass;
        pipeline_ci.basePipelineHandle;
        pipeline_ci.basePipelineIndex;

        VKCHECK(vkCreateGraphicsPipelines(device, 0, 1, &pipeline_ci, NULL, pipeline));
    }







    void SetPushConstants(PushConstants* constants)
    {
        vkCmdPushConstants(gGraphics_cmd_buffer_in_flight, gDeviceContext.default_graphics_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), constants);
    }

    void SetObjectData(ObjectData* object_data, uint32_t object_idx)
    {
        //for (size_t i = 0; i < 128; i++) {
        //    ((ObjectData*)mapped_object_data_ptrs[gFrame_in_flight_idx])[object_idx].joint_matrices[i] = object_data->joint_matrices[i];
        //}

        memcpy(
            ((ObjectData*)gDeviceContext.mapped_object_data_ptrs[gFrame_in_flight_idx])[object_idx].joint_matrices,
            object_data->joint_matrices,
            sizeof(glm::mat4) * MAX_JOINTS);

        ((ObjectData*)gDeviceContext.mapped_object_data_ptrs[gFrame_in_flight_idx])[object_idx].model_matrix = object_data->model_matrix;
    }

    void SetWindowSize(int width, int height)
    {
        if (width <= 0 || height <= 0) {
            SDL_Log("width and height must be greater than 0");
            return;
        }

        gDevice.width = width;
        gDevice.height = height;
    }

    void GetWindowSize(int* width, int* height)
    {
        *width = gDevice.width;
        *height = gDevice.height;
    }

    void SetActiveCamera(Camera* camera)
    {
        if (!camera) {
            SDL_Log("trying to set NULL camera ptr");
            return;
        }

        gActive_camera = camera;
    }

    SDL_Window* GetSDLWindowHandle()
    {
        return gDevice.window;
    };

    void EnableDepthTest(bool enabled)
    {
        vkCmdSetDepthTestEnable(gGraphics_cmd_buffer_in_flight, enabled);
    }

    void EnableDepthWrite(bool enabled)
    {
        vkCmdSetDepthWriteEnable(gGraphics_cmd_buffer_in_flight, enabled);
    }

    void SetCullMode(VkCullModeFlags cullmode)
    {
        vkCmdSetCullMode(gGraphics_cmd_buffer_in_flight, cullmode);
    }

    void RebuildGraphicsPipeline()
    {
        CreateGraphicsPipeline(gDevice.device, &gDeviceContext.default_graphics_pipeline);
    }

    void DeviceWaitIdle()
    {
        vkDeviceWaitIdle(gDevice.device);
    }
}




/////////////////////////////////
// vulkan utility functions

// Allocates one primary command buffer from global graphics command pool
// assumed to handle transfer operations



void Swapchain::Create(VkSurfaceKHR surface, VkSwapchainKHR old_swapchain)
{

    // fixme: double buffering may not be supported on some platforms
    // call to check vkGetPhysicalDeviceSurfaceCapabilitiesKHR()

    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gDevice.physical_device, surface, &surface_capabilities));

    uint32_t surface_format_count = 0;
    VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gDevice.physical_device, surface, &surface_format_count, NULL));
    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gDevice.physical_device, surface, &surface_format_count, &surface_formats[0]));

    gDevice.width = surface_capabilities.currentExtent.width;
    gDevice.height = surface_capabilities.currentExtent.height;

    if (gDevice.width == 0 || gDevice.height == 0) return;

    // fixme: check with surface_capabilities.min/maxImageCount
    _image_count = 2;
    uint32_t usage_flag;

    if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        usage_flag = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    else {
        SDL_Log("Surface does not support COLOR_ATTACHMENT !");
        return;
    }

    _format = surface_formats[0].format;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = surface;
    swapchain_create_info.minImageCount = _image_count;
    swapchain_create_info.imageFormat = surface_formats[0].format;
    swapchain_create_info.imageColorSpace = surface_formats[0].colorSpace;
    swapchain_create_info.imageExtent = surface_capabilities.currentExtent; // fixme: needs to be checked
    swapchain_create_info.imageArrayLayers = 1; // we are not doing stereo rendering
    swapchain_create_info.imageUsage = usage_flag;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // only ever using swapchain _images with the same queue family
    swapchain_create_info.queueFamilyIndexCount = 1;
    swapchain_create_info.pQueueFamilyIndices = NULL;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform; // fixme: we sure?
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // we dont want to blend our window with other windows
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE; // we don't care about pixels covered by another window
    swapchain_create_info.oldSwapchain = old_swapchain;

    VkResult result = vkCreateSwapchainKHR(gDevice.device, &swapchain_create_info, NULL, &_handle);
    VKCHECK(result);




    //
    // Query swapchain _images
    //
    if (vkGetSwapchainImagesKHR(gDevice.device, _handle, &_image_count, NULL) != VK_SUCCESS) {
        SDL_Log("failed to retrieve swapchain images!");
        return;
    }
    _images.resize(_image_count);
    VKCHECK(vkGetSwapchainImagesKHR(gDevice.device, _handle, &_image_count, &_images[0]));




    //
    // swapchain image views
    //
    _image_views.resize(_image_count);

    for (size_t i = 0; i < _image_count; i++) {
        VkImageViewCreateInfo view_ci = {};
        view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image = _images[i];
        view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format = _format;

        view_ci.components.r = VK_COMPONENT_SWIZZLE_R;
        view_ci.components.g = VK_COMPONENT_SWIZZLE_G;
        view_ci.components.b = VK_COMPONENT_SWIZZLE_B;
        view_ci.components.a = VK_COMPONENT_SWIZZLE_A;
        view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.baseMipLevel = 0;
        view_ci.subresourceRange.levelCount = 1;
        view_ci.subresourceRange.baseArrayLayer = 0;
        view_ci.subresourceRange.layerCount = 1;
        VKCHECK(vkCreateImageView(gDevice.device, &view_ci, NULL, &_image_views[i]));
    }


    // Release MSAA and Depth image allocations
    if (old_swapchain != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(gDevice.device);

        vkDestroyImageView(gDevice.device, _msaa_image_view, nullptr);
        vmaDestroyImage(gDeviceContext.allocator, _msaa_image.handle, _msaa_image.vma_allocation);

        vkDestroyImageView(gDevice.device, _depth_image_view, nullptr);
        vmaDestroyImage(gDeviceContext.allocator, _depth_image.handle, _depth_image.vma_allocation);
    }


    //
    // MSAA image
    //

    VkImageCreateInfo msaa_image_ci{};
    msaa_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    msaa_image_ci.imageType = VK_IMAGE_TYPE_2D;
    msaa_image_ci.format = _format;

    msaa_image_ci.extent.width = gDevice.width;
    msaa_image_ci.extent.height = gDevice.height;
    msaa_image_ci.extent.depth = 1;

    msaa_image_ci.mipLevels = 1;
    msaa_image_ci.arrayLayers = 1;
    msaa_image_ci.samples = sample_count;
    msaa_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    msaa_image_ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    msaa_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    msaa_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo vma_msaa_image_alloc_ci{};
    vma_msaa_image_alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vma_msaa_image_alloc_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VKCHECK(vmaCreateImage(gDeviceContext.allocator, &msaa_image_ci, &vma_msaa_image_alloc_ci, &_msaa_image.handle, &_msaa_image.vma_allocation, NULL));

    VkImageViewCreateInfo resolve_view_ci{};
    resolve_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    resolve_view_ci.image = _msaa_image.handle;
    resolve_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    resolve_view_ci.format = _format;

    resolve_view_ci.components.r = VK_COMPONENT_SWIZZLE_R;
    resolve_view_ci.components.g = VK_COMPONENT_SWIZZLE_G;
    resolve_view_ci.components.b = VK_COMPONENT_SWIZZLE_B;
    resolve_view_ci.components.a = VK_COMPONENT_SWIZZLE_A;
    resolve_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    resolve_view_ci.subresourceRange.levelCount = 1;
    resolve_view_ci.subresourceRange.layerCount = 1;
    VKCHECK(vkCreateImageView(gDevice.device, &resolve_view_ci, NULL, &_msaa_image_view));


    //
    // Depth image
    //

    VkImageCreateInfo depth_image_ci{};
    depth_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_image_ci.imageType = VK_IMAGE_TYPE_2D;
    depth_image_ci.format = VK_FORMAT_D16_UNORM;

    depth_image_ci.extent.width = gDevice.width;
    depth_image_ci.extent.height = gDevice.height;
    depth_image_ci.extent.depth = 1;

    depth_image_ci.mipLevels = 1;
    depth_image_ci.arrayLayers = 1;
    depth_image_ci.samples = sample_count;
    depth_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL; // use VK_IMAGE_TILING_LINEAR if CPU writes are intended
    depth_image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // depth_image_ci.queueFamilyIndexCount;
    // depth_image_ci.pQueueFamilyIndices;
    depth_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo vma_depth_alloc_ci{};
    vma_depth_alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vma_depth_alloc_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VKCHECK(vmaCreateImage(gDeviceContext.allocator, &depth_image_ci, &vma_depth_alloc_ci, &_depth_image.handle, &_depth_image.vma_allocation, NULL));

    VkImageViewCreateInfo depth_view_ci{};
    depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_ci.image = _depth_image.handle;
    depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_ci.format = VK_FORMAT_D16_UNORM;

    depth_view_ci.components.r = VK_COMPONENT_SWIZZLE_R;
    depth_view_ci.components.g = VK_COMPONENT_SWIZZLE_G;
    depth_view_ci.components.b = VK_COMPONENT_SWIZZLE_B;
    depth_view_ci.components.a = VK_COMPONENT_SWIZZLE_A;
    depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_ci.subresourceRange.levelCount = 1;
    depth_view_ci.subresourceRange.layerCount = 1;
    VKCHECK(vkCreateImageView(gDevice.device, &depth_view_ci, NULL, &_depth_image_view));
}
