// #pragma once

// #include "adgl.h"

// struct Point
// {
//     float     position[3];
//     uint32_t  VAO;
//     uint32_t  VBO;
//     Material *material;
//
//     void DrawPoint(Point &p, glm::vec4 color);
//     void Create();
// };

// void Point::Create()
// {

//     point = {
//         .position = { 0.f, 0.f, 0.f },
//         .material = gMaterials["standard"]
//     };

//     glGenVertexArrays(1, &point.VAO);
//     glBindVertexArray(point.VAO);

//     glGenBuffers(1, &point.VBO);
//     glBindBuffer(GL_ARRAY_BUFFER, point.VBO);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(point.position), point.position, GL_STATIC_DRAW);

//     glEnableVertexAttribArray(0);
//     glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
// }

// void Point::DrawPoint(Point &p, glm::vec4 color)
// {
//     ShaderUse(p.material->_shader->programID);
//     glm::mat4 model = glm::mat4(1);
//     model           = glm::translate(model, { 1, 1, 1 });
//     ShaderSetMat4ByName("projection", gCameraInUse->_projection, p.material->_shader->programID);
//     ShaderSetMat4ByName("view", gCameraInUse->_view, p.material->_shader->programID);
//     ShaderSetMat4ByName("model", model, p.material->_shader->programID);
//     glEnable(GL_PROGRAM_POINT_SIZE);
//     glPointSize(60.f);
//     glBindVertexArray(p.VAO);
//     glDrawArrays(GL_POINTS, 0, 1);
// }