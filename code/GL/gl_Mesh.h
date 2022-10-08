#pragma once

struct Material
{
    char    *name {};
    Shader  *_shader {};
    Texture *base_color_map {};
};

static Material *MaterialCreate(const char *shaderPath, Texture *texture)
{
    Material *m = (Material *)malloc(sizeof(Material));
    assert(m);

    m->base_color_map = texture;

    if (gShaders.contains(shaderPath)) {
        m->_shader = gShaders[shaderPath];
    } else {
        if (!(m->_shader = LoadShader(shaderPath))) {
            // should we default to a default shader upon fail?
            SDL_Log("shader compilation failed [%s]\n", shaderPath);
            free(m);
            return NULL;
        }
        gShaders.insert({ (shaderPath), m->_shader });
        SDL_Log("[%d] loading [%s]", gShaders.size(), shaderPath);
    }


    return m;
}

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

struct MeshData
{
    void *ptr;
};

struct SkinnedModel
{

    ///////////////
    std::vector<Joint>       _joints;
    std::vector<AnimationV2> _animations_v2;

    ///////////////
    struct skinnedMesh
    {
        std::vector<Material *> _materials     = {}; // todo: store an index instead of a ptr
        std::vector<uint32_t>   _VAOs          = {};
        std::vector<uint64_t>   _indices_count = {};
        std::vector<uint64_t>   _index_offsets = {};
        std::string             _name;

        bool hidden = {};
    };
    char *assetFolder = {};

    std::vector<skinnedMesh> _meshes = {};
    std::vector<Animation>   _animations;
    Animation               *_current_animation     = {};
    bool                     _should_play_animation = true;
    Transform                _transform             = {}; // note: we do not load the exported transform
    MeshData                 _mesh_data             = {}; // opaque handle to cgltf_data
    uint32_t                 _data_buffer_id        = {};
    void                     Create(const char *path);
    void                     AnimationUpdate(float dt);
    void                     Draw(float dt);

    float phi = 0; // debug
};

void SkinnedModel::Draw(float dt)
{
    glm::mat4 model = glm::mat4(1);

    model = glm::translate(model, _transform.translation)
        * glm::rotate(model, (Radians(_transform.rotation.z)), glm::vec3(0, 0, 1))
        * glm::rotate(model, (Radians(_transform.rotation.y)), glm::vec3(0, 1, 0))
        * glm::rotate(model, (Radians(_transform.rotation.x)), glm::vec3(1, 0, 0))
        * glm::scale(model, glm::vec3(_transform.scale));


    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _data_buffer_id);

    phi += dt * 1.f;
    phi                = fmodf(phi, 360);
    glm::vec3 lightDir = { cosf(phi), -.5, sinf(phi) * .1f };

    for (size_t mesh_idx = 0; mesh_idx < this->_meshes.size(); mesh_idx++) {

        if (_meshes[mesh_idx].hidden) continue;

        for (size_t submesh_idx = 0; submesh_idx < this->_meshes[mesh_idx]._VAOs.size(); submesh_idx++) {

            Material *material = _meshes[mesh_idx]._materials[submesh_idx];
            ShaderUse(material->_shader->programID);
            ShaderSetMat4ByName("projection", gCameraInUse->_projection, material->_shader->programID);
            ShaderSetMat4ByName("view", gCameraInUse->_view, material->_shader->programID);
            ShaderSetMat4ByName("model", model, material->_shader->programID);
            ShaderSetUniformVec3ByName("view_pos", &gCameraInUse->_position, material->_shader->programID);
            ShaderSetUniformVec3ByName("light_dir", &lightDir, material->_shader->programID);


            if (_animations.size() > 0 && _current_animation)
                ShaderSetMat4ByName("joint_matrices", _current_animation->joint_matrices[0], _current_animation->joint_matrices.size(), material->_shader->programID);

            int has_joint_matrices = (_should_play_animation ? 1 : 0);
            ShaderSetUniformIntByName("has_joint_matrices", &has_joint_matrices, material->_shader->programID);

            // bind textures here
            // glActiveTexture(GL_TEXTURE0);
            // glBindTexture(GL_TEXTURE_2D, material->base_color_map->id);

            glBindVertexArray(_meshes[mesh_idx]._VAOs[submesh_idx]);
            glDrawElements(GL_TRIANGLES, _meshes[mesh_idx]._indices_count[submesh_idx], GL_UNSIGNED_SHORT, (void *)_meshes[mesh_idx]._index_offsets[submesh_idx]);
            glBindVertexArray(0);
        }
    }

    // glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void SkinnedModel::Create(const char *path)
{
    if (gSharedMeshes.contains(path)) {
        _mesh_data.ptr = gSharedMeshes[path];

    } else {
        cgltf_data *data;
        LoadCgltfData(path, &data);

        gSharedMeshes.insert({ std::string(path), (void *)data });
        _mesh_data.ptr = (void *)data;
    }

    cgltf_data *data = (cgltf_data *)_mesh_data.ptr;
    GetPathFolder(&assetFolder, path, strlen(path));

    // todo: we do take yet take into account the exported model transform of the mesh

    glGenBuffers(1, &_data_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, _data_buffer_id);


    // assert(data->buffers_count == 1);
    // note: currently we allocate more data on the GPU than we really need to render this mesh
    // we allocate for the entire .bin which contains more stuff than just primitives & indices
    size_t total_data_size_for_current_submesh = data->buffers->size;

    glBufferData(GL_ARRAY_BUFFER, total_data_size_for_current_submesh, 0, GL_DYNAMIC_DRAW);
    size_t totalSize = 0;

    _meshes.resize(data->meshes_count);
    for (size_t mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++) {
        cgltf_primitive *primitives = data->meshes[mesh_idx].primitives;

        size_t submeshCount = data->meshes[mesh_idx].primitives_count;
        _meshes[mesh_idx]._VAOs.resize(submeshCount);
        _meshes[mesh_idx]._indices_count.resize(submeshCount);
        _meshes[mesh_idx]._index_offsets.resize(submeshCount);
        _meshes[mesh_idx]._materials.resize(submeshCount);

        glGenVertexArrays(submeshCount, &_meshes[mesh_idx]._VAOs[0]);

        for (size_t submesh_idx = 0; submesh_idx < submeshCount; submesh_idx++) {
            cgltf_primitive *primitive = primitives + submesh_idx;

            glBindVertexArray(_meshes[mesh_idx]._VAOs[submesh_idx]);
            glBindBuffer(GL_ARRAY_BUFFER, _data_buffer_id);

            for (size_t attrib_idx = 0; attrib_idx < primitives->attributes_count; attrib_idx++) {
                cgltf_attribute   *attrib = primitives[submesh_idx].attributes + attrib_idx;
                cgltf_buffer_view *view   = attrib->data->buffer_view;

                glBufferSubData(GL_ARRAY_BUFFER, view->offset, view->size, (void *)((uint8_t *)view->buffer->data + view->offset));

                totalSize += view->size;
                if (attrib->type == cgltf_attribute_type_position) {
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_normal) {
                    glEnableVertexAttribArray(1);
                    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_texcoord && strcmp(attrib->name, "TEXCOORD_0") == 0) {
                    glEnableVertexAttribArray(2);
                    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_texcoord && strcmp(attrib->name, "TEXCOORD_1") == 0) {
                    glEnableVertexAttribArray(3);
                    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_joints) {
                    glEnableVertexAttribArray(4);
                    glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_weights) {
                    glEnableVertexAttribArray(5);
                    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                }
            }



            totalSize += primitive->indices->buffer_view->size;
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _data_buffer_id);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                primitive->indices->buffer_view->offset,
                primitive->indices->buffer_view->size,
                ((uint8_t *)primitive->indices->buffer_view->buffer->data + primitive->indices->buffer_view->offset));

            _meshes[mesh_idx]._index_offsets[submesh_idx] = primitive->indices->buffer_view->offset;
            _meshes[mesh_idx]._indices_count[submesh_idx] = primitive->indices->count;


            // Retrieve Texture info
            // load textures into global map and store the ids



            if (!primitive->material) {
                _meshes[mesh_idx]._materials[submesh_idx] = gMaterials["default"];
                continue;
            };

            auto mat = primitive->material;

            if (mat->has_pbr_specular_glossiness) {
                SDL_Log("[%s] has specular_glossiness workflow, which is not supported", data->meshes[mesh_idx].name);
                _meshes[mesh_idx]._materials[submesh_idx] = gMaterials["default"];
            } else if (mat->has_pbr_metallic_roughness) {
                // mat->pbr_metallic_roughness.base_color_texture.texcoord;
                if (mat->pbr_metallic_roughness.base_color_texture.texture) {

                    if (mat->pbr_metallic_roughness.base_color_texture.texture->image->name) {
                        cgltf_texture *texture = mat->pbr_metallic_roughness.base_color_texture.texture;

                        // check if a texture by that name already exist in gTextures
                        // if it does, material->texture = gTextures[name];
                        // else load the texture into gTextures then material->texture = gTextures[name];
                        if (gTextures.contains(std::string(texture->image->uri))) {
                            _meshes[mesh_idx]._materials[submesh_idx] = MaterialCreate("shaders/standard.shader", gTextures[texture->image->uri]);
                        } else {
                            // todo: sampler->min/mag and stuff
                            // gTextures.insert({ texture->image->name, TextureCreate((std::string(assetFolder) + std::string(texture->image->uri)).c_str()) });
                            auto t = TextureCreate((std::string(assetFolder) + std::string(texture->image->uri)).c_str());
                            gTextures.insert({ std::string(texture->image->uri), t });
                            _meshes[mesh_idx]._materials[submesh_idx] = MaterialCreate("shaders/standard.shader", t);
                        }
                    }
                } else {
                    // set default material
                    _meshes[mesh_idx]._materials[submesh_idx] = gMaterials["default"];
                }
            }
        }
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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





    ////////// todo: figure out if we should actually go this route
    ////// Process animation samples
    auto anims_data = data->animations;

    _animations_v2.resize(data->animations_count);
    for (size_t anim_idx = 0; anim_idx < data->animations_count; anim_idx++) {
        int sample_count = AnimationMaxSampleCount(anims_data);
        _animations_v2[anim_idx].samples.resize(skin->joints_count);

        // For each Joint
        for (size_t joint_idx = 0; joint_idx < skin->joints_count; joint_idx++) {

            // For each Channel
            for (size_t chan_idx = 0; chan_idx < anims_data->channels_count; chan_idx++) {
                cgltf_animation_channel *channel = &anims_data->channels[chan_idx];

                // if channel target matches current joint
                if (channel->target_node == skin->joints[joint_idx]) {

                    cgltf_animation_sampler *sampler = channel->sampler;

                    _animations_v2[anim_idx].samples[joint_idx].target_joint = joint_idx;

                    auto   input_data_ptr    = (uint8_t *)sampler->input->buffer_view->buffer->data;
                    auto   input_data_offset = sampler->input->buffer_view->offset;
                    float *timestamps_data   = (float *)(input_data_ptr + input_data_offset);

                    auto   output_data_ptr     = (uint8_t *)sampler->output->buffer_view->buffer->data;
                    auto   output_data_offset  = sampler->output->buffer_view->offset;
                    float *transformation_data = (float *)(output_data_ptr + output_data_offset);

                    if (channel->target_path == cgltf_animation_path_type_translation) {
                        _animations_v2[anim_idx].samples[joint_idx].translation_channel.timestamps   = timestamps_data;
                        _animations_v2[anim_idx].samples[joint_idx].translation_channel.translations = (glm::vec3 *)transformation_data;

                    } else if (channel->target_path == cgltf_animation_path_type_rotation) {
                        _animations_v2[anim_idx].samples[joint_idx].rotation_channel.timestamps = timestamps_data;
                        _animations_v2[anim_idx].samples[joint_idx].rotation_channel.rotations  = (glm::quat *)transformation_data;
                    } else if (channel->target_path == cgltf_animation_path_type_scale) {
                        _animations_v2[anim_idx].samples[joint_idx].scale_channel.timestamps = timestamps_data;
                        _animations_v2[anim_idx].samples[joint_idx].scale_channel.scales     = (glm::vec3 *)transformation_data; // fixme: we might want to scope this to uniform scale only
                    }
                }
            } // End of channels loop for the current joint
        } // End of Joints loop
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
        bindPoseLocalJointTransforms[joint_idx].scale       = glm::vec3(1);

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
}
