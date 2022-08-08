#pragma once

#include "shader.h"
#include "assimp/Importer.hpp"
#include "mymath.h"
#include "texture.h"
#include "stb_image.h"
#include <vector>

struct Vertex
{
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;
};

struct Mesh
{
    std::vector<Vertex>   vertices;
    std::vector<uint16_t> indices;
    std::vector<Texture>  textures;
    uint32_t              VAO;
    uint32_t              VBO;
    uint32_t              IBO;
    Matrix4               transform;

    void Create(std::vector<Vertex> vertices, std::vector<uint16_t> indices)
    {
        this->vertices = vertices;
        this->indices = indices;

        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex) /*bytes*/, vertices.data(), GL_DYNAMIC_DRAW);

        glGenBuffers(1, &IBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t) /*bytes*/, indices.data(), GL_DYNAMIC_DRAW);

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
};