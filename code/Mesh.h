#pragma once

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec4 joints;
    glm::vec4 weights;
};

static float triangle_verts[] = {
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.0f, 0.5f, -0.5f
};

static uint16_t triangle_indices[] = {
    0, 1, 2
};

static std::vector<Vertex> cube_verts = {
    // front
    { { -1.0, -1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 0.0 } },
    { { 1.0, -1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 1.0, 0.0 } },
    { { 1.0, 1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 1.0, 1.0 } },
    { { -1.0, 1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0 } },
    // back
    { { -1.0, -1.0, -1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 0.0 } },
    { { 1.0, -1.0, -1.0 }, { 1.0, 1.0, 1.0 }, { 1.0, 0.0 } },
    { { 1.0, 1.0, -1.0 }, { 1.0, 1.0, 1.0 }, { 1.0, 1.0 } },
    { { -1.0, 1.0, -1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0 } }
};

static std::vector<uint16_t> cube_indices = {
    // front
    0, 1, 2,
    2, 3, 0,
    // right
    1, 5, 6,
    6, 2, 1,
    // back
    7, 6, 5,
    5, 4, 7,
    // left
    4, 0, 3,
    3, 7, 4,
    // bottom
    4, 5, 1,
    1, 0, 4,
    // top
    3, 2, 6,
    6, 7, 3
};





struct Material
{
    char    *name {};
    Shader  *_shader {};
    Texture *baseColorMap {};
};

static Material *MaterialCreate(const char *shaderPath, Texture *texture)
{
    Material *m = (Material *)malloc(sizeof(Material));
    assert(m);

    m->baseColorMap = texture;

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
    }


    return m;
}





//---------------------------------------
// Interleaved Meshes implementation
//---------------------------------------

struct Mesh
{
    std::vector<std::vector<Vertex>>   _submeshVertices;
    std::vector<std::vector<uint16_t>> _submeshIndices;
    std::vector<std::vector<Texture>>  _subTextures;



    std::vector<uint32_t> _VAOs;
    std::vector<uint32_t> _VBOs;
    std::vector<uint32_t> _IBOs;

    int32_t _submeshCount;

    std::vector<Material *> _submeshMaterials;

    void Create();
};

void Mesh::Create()
{
    _VAOs.resize(_submeshCount);
    _VBOs.resize(_submeshCount);
    _IBOs.resize(_submeshCount);

    for (size_t i = 0; i < _submeshVertices.size(); i++) {

        glGenVertexArrays(1, &_VAOs[i]);
        glBindVertexArray(_VAOs[i]);

        glGenBuffers(1, &_VBOs[i]);
        glBindBuffer(GL_ARRAY_BUFFER, _VBOs[i]);
        glBufferData(GL_ARRAY_BUFFER, _submeshVertices[i].size() * sizeof(Vertex) /*bytes*/, _submeshVertices[i].data(), GL_DYNAMIC_DRAW);

        glGenBuffers(1, &_IBOs[i]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _IBOs[i]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, _submeshIndices[i].size() * sizeof(uint16_t) /*bytes*/, _submeshIndices[i].data(), GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0 /*layout location*/,
            3 /*number of components*/,
            GL_FLOAT /*type of components*/,
            GL_FALSE,
            sizeof(Vertex),
            (void *)offsetof(Vertex, Vertex::position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1 /*layout location*/,
            3 /*number of components*/,
            GL_FLOAT /*type of components*/,
            GL_FALSE,
            sizeof(Vertex),
            (void *)offsetof(Vertex, Vertex::normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2 /*layout location*/,
            2 /*number of components*/,
            GL_FLOAT /*type of components*/,
            GL_FALSE,
            sizeof(Vertex),
            (void *)offsetof(Vertex, Vertex::texCoord));

        glBindVertexArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}


struct Model
{
    std::vector<Mesh> _meshes;
    glm::mat4         _transform;
    int32_t           _meshCount;

    void Create(const char *path)
    {

        cgltf_options options = {};
        cgltf_data   *data    = NULL;
        cgltf_result  result  = cgltf_parse_file(&options, path, &data);

        if (result == cgltf_result_success) {
            result = cgltf_load_buffers(&options, data, path);

            if (data->meshes_count == 0) {
                // todo: make this format into a LOG_ERROR() macro
                SDL_Log("%s:%d: no meshes found...", __FILE__, __LINE__);
            }
        }

        _meshes.resize(data->meshes_count);
        _meshCount = (int32_t)data->meshes_count;

        for (size_t i = 0; i < data->meshes_count; i++) {
            uint32_t submeshCount = data->meshes[i].primitives_count;

            _meshes[i]._submeshVertices.resize(submeshCount);
            _meshes[i]._submeshIndices.resize(submeshCount);
            _meshes[i]._submeshCount = submeshCount;
            _meshes[i]._submeshMaterials.resize(submeshCount);

            for (size_t j = 0; j < submeshCount; j++) {
                {
                    // todo: need to check for which primitives are available
                    cgltf_buffer_view *positions = data->meshes[i].primitives[j].attributes[0].data->buffer_view;
                    cgltf_buffer_view *normals   = data->meshes[i].primitives[j].attributes[1].data->buffer_view;
                    cgltf_buffer_view *texCoords = data->meshes[i].primitives[j].attributes[2].data->buffer_view;

                    for (size_t a = 0; a < data->meshes[i].primitives[j].attributes[i].data->count; a++) {
                        Vertex vert;
                        vert.position = ((glm::vec3 *)(((char *)positions->buffer->data) + positions->offset))[a];
                        vert.normal   = ((glm::vec3 *)(((char *)normals->buffer->data) + normals->offset))[a];
                        vert.texCoord = ((glm::vec2 *)(((char *)texCoords->buffer->data) + texCoords->offset))[a];
                        _meshes[i]._submeshVertices[j].push_back(vert);
                    }
                }

                cgltf_buffer_view *indices_view = data->meshes[i].primitives[j].indices->buffer_view;
                for (size_t b = 0; b < data->meshes[i].primitives[j].indices->count; b++) {
                    _meshes[i]._submeshIndices[j].push_back(((uint16_t *)((char *)indices_view->buffer->data + indices_view->offset))[b]);
                }
            }

            _meshes[i].Create();
        }
    }
};



//---------------------------------------
// Non-interleaved Mesh implementation
//---------------------------------------



struct Animation
{
    void       *handle      = {};
    const char *name        = {};
    float       duration    = {};
    float       globalTimer = {};
    bool        isPlaying {};

    uint8_t      jointCount;
    cgltf_node **joints;

    std::vector<glm::mat4> finalPoseJointMatrices;
    std::vector<glm::mat4> currentPoseJointMatrices;
    std::vector<Transform> currentPoseJointTransforms;
};

struct MeshData
{
    void *ptr;
};

struct SkinnedModel
{
    struct skinnedMesh
    {
        std::vector<Material *> _materials      = {}; // todo: store an index instead of a ptr
        std::vector<uint32_t>   _VAOs           = {};
        std::vector<uint64_t>   _indicesCount   = {};
        std::vector<uint64_t>   _indicesOffsets = {};
        std::string             _name;
        char                   *_textureKeyBaseColor = {};

        bool hidden = {};
    };
    char *assetFolder = {};

    std::vector<skinnedMesh> _meshes = {};
    std::vector<Animation>   _animations;
    Animation               *_currentAnimation    = {};
    bool                     _shouldPlayAnimation = true;
    Transform                _transform           = {}; // note: we do not load the exported transform
    MeshData                 _meshData            = {}; // opaque handle to cgltf_data
    uint32_t                 _DataBufferID        = {};
    void                     Create(const char *path);
    void                     AnimationUpdate(float dt);
    void                     Draw(float dt);
};

void SkinnedModel::Draw(float dt)
{
    glm::mat4 model = glm::mat4(1);

    model = glm::translate(model, _transform.translation)
        * glm::rotate(model, (Radians(_transform.rotation.z)), glm::vec3(0, 0, 1))
        * glm::rotate(model, (Radians(_transform.rotation.y)), glm::vec3(0, 1, 0))
        * glm::rotate(model, (Radians(_transform.rotation.x)), glm::vec3(1, 0, 0))
        * glm::scale(model, glm::vec3(_transform.scale));


    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _DataBufferID);

    for (size_t mesh_idx = 0; mesh_idx < this->_meshes.size(); mesh_idx++) {

        if (_meshes[mesh_idx].hidden) continue;

        for (size_t submesh_idx = 0; submesh_idx < this->_meshes[mesh_idx]._VAOs.size(); submesh_idx++) {

            Material *material = _meshes[mesh_idx]._materials[submesh_idx];
            ShaderUse(material->_shader->programID);
            ShaderSetMat4ByName("projection", gCameraInUse->_projection, material->_shader->programID);
            ShaderSetMat4ByName("view", gCameraInUse->_view, material->_shader->programID);
            ShaderSetMat4ByName("model", model, material->_shader->programID);
            ShaderSetUniformVec3ByName("view_pos", &gCameraInUse->_position, material->_shader->programID);

            static float phi = 0;
            phi += dt*0.1f;
            glm::vec3 lightDir = { cosf(phi), -.5, sinf(phi)*.1f };
            ShaderSetUniformVec3ByName("light_dir", &lightDir, material->_shader->programID);


            if (_animations.size() > 0 && _currentAnimation)
                ShaderSetMat4ByName("finalPoseJointMatrices", _currentAnimation->finalPoseJointMatrices[0], _currentAnimation->finalPoseJointMatrices.size(), material->_shader->programID);

            int has_joint_matrices = (_shouldPlayAnimation ? 1 : 0);
            ShaderSetUniformIntByName("has_joint_matrices", &has_joint_matrices, material->_shader->programID);

            // bind textures here
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, material->baseColorMap->id);

            glBindVertexArray(_meshes[mesh_idx]._VAOs[submesh_idx]);
            glDrawElements(GL_TRIANGLES, _meshes[mesh_idx]._indicesCount[submesh_idx], GL_UNSIGNED_SHORT, (void *)_meshes[mesh_idx]._indicesOffsets[submesh_idx]);
            glBindVertexArray(0);
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void SkinnedModel::Create(const char *path)
{
    if (gSharedMeshes.contains(path)) {
        _meshData.ptr = gSharedMeshes[path];

    } else {
        cgltf_data   *data;
        cgltf_options options = {};
        cgltf_result  result  = cgltf_parse_file(&options, path, &data);

        if (cgltf_parse_file(&options, path, &data) == cgltf_result_success) {
            if (cgltf_load_buffers(&options, data, path) == cgltf_result_success) {
                if ((result = cgltf_validate(data)) != cgltf_result_success) {
                    SDL_Log("cgltf validation error");
                    return;
                }
                result = cgltf_load_buffers(&options, data, path);
            }
        } else {
            SDL_Log("Failed to load [%s]", path);
            return;
        }

        gSharedMeshes.insert({ std::string(path), (void *)data });
        _meshData.ptr = (void *)data;
    }

    cgltf_data *data = (cgltf_data *)_meshData.ptr;
    GetPathFolder(&assetFolder, path, strlen(path));

    // todo: we do take yet take into account the exported model transform of the mesh

    glGenBuffers(1, &_DataBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, _DataBufferID);


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
        _meshes[mesh_idx]._indicesCount.resize(submeshCount);
        _meshes[mesh_idx]._indicesOffsets.resize(submeshCount);
        _meshes[mesh_idx]._materials.resize(submeshCount);

        glGenVertexArrays(submeshCount, &_meshes[mesh_idx]._VAOs[0]);

        for (size_t submesh_idx = 0; submesh_idx < submeshCount; submesh_idx++) {
            cgltf_primitive *primitive = primitives + submesh_idx;

            glBindVertexArray(_meshes[mesh_idx]._VAOs[submesh_idx]);
            glBindBuffer(GL_ARRAY_BUFFER, _DataBufferID);

            // we only ever loop once through the primitives
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
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _DataBufferID);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                primitive->indices->buffer_view->offset,
                primitive->indices->buffer_view->size,
                ((uint8_t *)primitive->indices->buffer_view->buffer->data + primitive->indices->buffer_view->offset));

            _meshes[mesh_idx]._indicesOffsets[submesh_idx] = primitive->indices->buffer_view->offset;
            _meshes[mesh_idx]._indicesCount[submesh_idx]   = primitive->indices->count;


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
                            // SDL_Log("loading existing [%s] texture", texture->image->name);
                        } else {
                            // todo: sampler->min/mag and stuff
                            // gTextures.insert({ texture->image->name, TextureCreate((std::string(assetFolder) + std::string(texture->image->uri)).c_str()) });
                            auto t = TextureCreate((std::string(assetFolder) + std::string(texture->image->uri)).c_str());
                            gTextures.insert({ std::string(texture->image->uri), t });
                            _meshes[mesh_idx]._materials[submesh_idx] = MaterialCreate("shaders/standard.shader", t);
                            // SDL_Log("loading new [%s] texture", texture->image->uri);
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

        anim->jointCount = data->skins->joints_count;
        anim->joints     = data->skins->joints;
        anim->finalPoseJointMatrices.resize(anim->jointCount);
        anim->currentPoseJointMatrices.resize(anim->jointCount);
        anim->currentPoseJointTransforms.resize(anim->jointCount);

        for (size_t i = 0; i < anim->finalPoseJointMatrices.size(); i++) {
            anim->finalPoseJointMatrices[i] = glm::mat4(1);
        }
    }
}





void SkinnedModel::AnimationUpdate(float dt)
{

    if (!_shouldPlayAnimation) return;

    if (!_currentAnimation) {
        SDL_Log("CurrentAnimation not set");
        return;
    }

    assert(_currentAnimation->duration > 0);
    _currentAnimation->isPlaying = true;

    cgltf_animation *anim = (cgltf_animation *)_currentAnimation->handle;

    _currentAnimation->globalTimer += dt;
    float animTime = fmodf(_currentAnimation->globalTimer, _currentAnimation->duration);


    static int iterationNb = 0;

    if (iterationNb++ == 0) {
        SDL_Log("playing:[%s] | duration: %fsec", anim->name, _currentAnimation->duration);
    }

    // For each Joint
    for (size_t joint_idx = 0; joint_idx < _currentAnimation->jointCount; joint_idx++) {

        // For each Channel
        for (size_t chan_idx = 0; chan_idx < anim->channels_count; chan_idx++) {
            cgltf_animation_channel *channel = &anim->channels[chan_idx];

            // if channel target matches current joint
            if (channel->target_node == _currentAnimation->joints[joint_idx]) { // && target_path!!!

                int currentKey = -1;
                int nextKey    = -1;

                cgltf_animation_sampler *sampler = channel->sampler;
                for (size_t timestamp_idx = 0; timestamp_idx < sampler->input->count - 1; timestamp_idx++) {
                    float sampled_time;
                    float sampled_time_prev;

                    if ((sampled_time = readFloatFromAccessor(sampler->input, timestamp_idx + 1)) >= animTime) {
                        currentKey = timestamp_idx;
                        nextKey    = currentKey + 1;
                        break;
                    }
                }

                assert(currentKey >= 0 && nextKey >= 0);


                // todo: currentKey => nextKey interpolation
                // use slerp for quaternions
                if (channel->target_path == cgltf_animation_path_type_translation) {
                    _currentAnimation->currentPoseJointTransforms[joint_idx].translation = getVec3AtKeyframe(sampler, currentKey);
                } else if (channel->target_path == cgltf_animation_path_type_rotation) {
                    auto Q                                                            = getQuatAtKeyframe(sampler, currentKey);
                    _currentAnimation->currentPoseJointTransforms[joint_idx].rotation = glm::eulerAngles(Q);
                } else if (channel->target_path == cgltf_animation_path_type_scale) {
                    _currentAnimation->currentPoseJointTransforms[joint_idx].scale = 1.f;
                }

                _currentAnimation->currentPoseJointTransforms[joint_idx].name = _currentAnimation->joints[joint_idx]->name;



                if (joint_idx == 0) continue;

                if (_currentAnimation->joints[joint_idx]->parent == _currentAnimation->joints[joint_idx - 1]) {
                    _currentAnimation->currentPoseJointTransforms[joint_idx].parent       = &_currentAnimation->currentPoseJointTransforms[joint_idx - 1];
                    _currentAnimation->currentPoseJointTransforms[joint_idx].parent->name = _currentAnimation->currentPoseJointTransforms[joint_idx - 1].name;

                    // _currentAnimation->currentPoseJointTransforms[joint_idx - 1].child       = &_currentAnimation->currentPoseJointTransforms[joint_idx];
                    // _currentAnimation->currentPoseJointTransforms[joint_idx - 1].child->name = _currentAnimation->currentPoseJointTransforms[joint_idx].name;
                } else {
                    // find index of parent joint
                    auto currentJointParent = _currentAnimation->joints[joint_idx]->parent;
                    for (size_t i = 0; i < _currentAnimation->jointCount; i++) {
                        if (_currentAnimation->joints[i] == currentJointParent) {
                            _currentAnimation->currentPoseJointTransforms[joint_idx].parent       = &_currentAnimation->currentPoseJointTransforms[i];
                            _currentAnimation->currentPoseJointTransforms[joint_idx].parent->name = _currentAnimation->currentPoseJointTransforms[i].name;
                        }
                    }
                }
            }
        } // End of channels loop for the current joint




        // comppute global matrix for current joint
        if (joint_idx == 0) {
            _currentAnimation->currentPoseJointMatrices[0] = _currentAnimation->currentPoseJointTransforms[0].GetLocalMatrix();
        } else {
            _currentAnimation->currentPoseJointMatrices[joint_idx] = _currentAnimation->currentPoseJointTransforms[joint_idx].ComputeGlobalMatrix();
        }
        glm::mat4 *invBindMatrices                           = ((glm::mat4 *)((uint8_t *)((cgltf_data *)_meshData.ptr)->skins->inverse_bind_matrices->buffer_view->buffer->data + ((cgltf_data *)_meshData.ptr)->skins->inverse_bind_matrices->buffer_view->offset));
        _currentAnimation->finalPoseJointMatrices[joint_idx] = _currentAnimation->currentPoseJointMatrices[joint_idx] * invBindMatrices[joint_idx];
        // finalPoseJointMatrices[joint_idx]   = currentPoseJointMatrices[joint_idx] * glm::inverse(bindPoseLocalJointTransforms[joint_idx].ComputeGlobalMatrix());
    } // End of Joints loop
}