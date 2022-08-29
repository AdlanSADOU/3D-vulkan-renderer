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

static Material *MaterialCreate(const char*shaderPath, Texture *texture)
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

struct MeshSOA
{
    Vertex **_submeshVertices;
};


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
};

struct Model
{
    std::vector<Mesh> _meshes;
    glm::mat4         _transform;
    int32_t           _meshCount;

    void Create(const char *path)
    {

        cgltf_options options = {};
        cgltf_data *  data    = NULL;
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
            uint32_t primitivesCount = data->meshes[i].primitives_count;

            _meshes[i]._submeshVertices.resize(primitivesCount);
            _meshes[i]._submeshIndices.resize(primitivesCount);
            _meshes[i]._submeshCount = primitivesCount;
            _meshes[i]._submeshMaterials.resize(primitivesCount);

            for (size_t j = 0; j < primitivesCount; j++) {
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
