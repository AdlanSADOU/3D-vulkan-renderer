#pragma once

#include "shader.h"
#include "texture.h"

#include <stb_image.h>
#include <cgltf.h>
#include <vector>

struct Material
{
    Shader *_shader {};

    // todo(ad): we might want multiple textures
    Texture *_texture {};
};

static Material *MaterialCreate(const char *shaderPath, Texture *texture)
{
    Material *m = (Material *)malloc(sizeof(Material));

    m->_texture = texture;
    if (!(m->_shader = LoadShader(shaderPath))) {
        SDL_Log("shader prog failed\n");
        free(m);
        return NULL;
    }

    return m;
}

struct Mesh
{
    std::vector<std::vector<Vertex>>   _submeshVertices;
    std::vector<std::vector<uint16_t>> _submeshIndices;
    std::vector<std::vector<Texture>>  _subTextures;

    VertexSOA vertices;
    uint16_t *indices;
    Texture  *textures;


    std::vector<uint32_t> _VAOs;
    std::vector<uint32_t> _VBOs;
    std::vector<uint32_t> _IBOs;

    int32_t _submeshCount;

    std::vector<Material *> _submeshMaterials;

    void Create()
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

    void Create2()
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
};

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



//-------------------------------------------------------------------

struct Mesh2
{
    std::vector<Material *> _materials      = {};
    std::vector<uint32_t>   _VAOs           = {};
    std::vector<uint64_t>   _indicesCount   = {};
    std::vector<uint64_t>   _indicesOffsets = {};
};

struct Model2
{
    std::vector<Mesh2> _meshes       = {};
    uint32_t           _DataBufferID = {};
    Transform          _transform    = {};

    void Create(const char *path);
    void Draw();
};

void Model2::Draw()
{
}

void Model2::Create(const char *path)
{
    // note: this gltf data is an asset, as such there sould only exist one
    // instance of this in memory. So, instead of giving a path
    // make entries into a hash table
    cgltf_options options = {};
    cgltf_data   *data    = NULL;
    cgltf_result  result  = cgltf_parse_file(&options, path, &data);

    if (cgltf_parse_file(&options, path, &data) == cgltf_result_success) {
        if (cgltf_load_buffers(&options, data, path) == cgltf_result_success) {
            if ((result = cgltf_validate(data)) != cgltf_result_success) {
                SDL_Log("cgltf validation error");
                return;
            }
            if (data->meshes_count == 0) {
                // todo: make this format into a LOG_ERROR() macro
                SDL_Log("%s:%d: no meshes found...", __FILE__, __LINE__);
                return;
            }
        }
    }





    if (result == cgltf_result_success) {
        result = cgltf_load_buffers(&options, data, path);
    }

    _meshes.resize(data->meshes_count);
    for (size_t mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++) {
        cgltf_primitive *primitives = data->meshes[mesh_idx].primitives;

        size_t submeshCount = data->meshes[mesh_idx].primitives_count;
        _meshes[mesh_idx]._VAOs.resize(submeshCount);
        _meshes[mesh_idx]._indicesCount.resize(submeshCount);
        _meshes[mesh_idx]._indicesOffsets.resize(submeshCount);
        _meshes[mesh_idx]._materials.resize(submeshCount);


        glGenBuffers(1, &_DataBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, _DataBufferID);




        // get the total data size for this mesh: primitives + attributes
        size_t total_data_size_for_current_submesh = 0;
        for (size_t submesh_idx = 0; submesh_idx < submeshCount; submesh_idx++) {
            cgltf_primitive *primitive = primitives + submesh_idx;
            for (size_t i = 0; i < primitive->attributes_count; i++) {
                total_data_size_for_current_submesh += primitive->attributes[i].data->buffer_view->size;
            }
            total_data_size_for_current_submesh += primitive->indices->buffer_view->size;
        }
        // note: if a model has multiple meshes, assuming that multiple meshes share the same data blob at different offsets
        // this assert should then fire
        // assert(total_data_size_for_current_submesh == primitives->attributes->data->buffer_view->buffer->size);





        glBufferData(GL_ARRAY_BUFFER, total_data_size_for_current_submesh, 0, GL_DYNAMIC_DRAW);
        glGenVertexArrays(submeshCount, &_meshes[mesh_idx]._VAOs[mesh_idx]);

        for (size_t submesh_idx = 0; submesh_idx < submeshCount; submesh_idx++) {
            cgltf_primitive *primitive = primitives + submesh_idx;

            glBindBuffer(GL_ARRAY_BUFFER, _DataBufferID);
            glBindVertexArray(_meshes[mesh_idx]._VAOs[submesh_idx]);

            for (size_t attrib_idx = 0; attrib_idx < data->meshes->primitives->attributes_count; attrib_idx++) {
                cgltf_attribute   *attrib = primitives[submesh_idx].attributes + attrib_idx;
                cgltf_buffer_view *view   = attrib->data->buffer_view;

                glBufferSubData(GL_ARRAY_BUFFER, view->offset, view->size, (void *)((uint8_t *)view->buffer->data + view->offset));

                if (attrib->type == cgltf_attribute_type_position) {
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_normal) {
                    glEnableVertexAttribArray(1);
                    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_texcoord) {
                    glEnableVertexAttribArray(2);
                    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_joints) {
                    glEnableVertexAttribArray(3);
                    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                } else if (attrib->type == cgltf_attribute_type_weights) {
                    glEnableVertexAttribArray(4);
                    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 0, (void *)view->offset);
                }
            }

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _DataBufferID);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                primitive->indices->buffer_view->offset,
                primitive->indices->buffer_view->size,
                ((uint8_t *)primitive->indices->buffer_view->buffer->data + primitive->indices->buffer_view->offset));

            _meshes[mesh_idx]._indicesOffsets[submesh_idx] = primitive->indices->buffer_view->offset;
            _meshes[mesh_idx]._indicesCount[submesh_idx]   = primitive->indices->count;
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}