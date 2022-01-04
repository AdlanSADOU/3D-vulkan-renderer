#include "shader.h"

#include <stdio.h>
#include <stdlib.h>

#include <adgl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern uint32_t WND_WIDTH;
extern uint32_t WND_HEIGHT;
extern Controls controls;

Shader        vert_shader;
Shader        frag_shader;
ShaderProgram default_shader_program;

glm::mat4 view;
glm::mat4 projection;

glm::vec3 camera_position;

GLuint vao_quad;
GLuint vbo_quad;
GLuint ibo_quad;
GLuint texture;
int    tex_width, tex_height, nrChannels;

GLuint vao_quad2;
GLuint vbo_quad2;
GLuint ibo_quad2;

Quad      quad;
glm::mat4 quad_transform;
glm::vec2 quad_pos = {};

Quad      quad2;
glm::mat4 quad_transform2;
glm::vec2 quad_pos2;


void GLInit()
{
    stbi_set_flip_vertically_on_load(true);

    vert_shader.CreateAndCompile("./shaders/vertexShader.vert", GL_VERTEX_SHADER);
    frag_shader.CreateAndCompile("./shaders/fragmentShader.frag", GL_FRAGMENT_SHADER);
    default_shader_program.AddShader(&vert_shader);
    default_shader_program.AddShader(&frag_shader);
    default_shader_program.CreateAndLinkProgram();
    default_shader_program.UseProgram();

    GLint vPos_attrib_location      = 0;
    GLint vColor_attrib_location    = 1;
    GLint vTexCoord_attrib_location = 2;

    //////////////////////////////////////
    /// Texture
    char texture_path[] = "assets/powerup.png";

    unsigned char *pixels = stbi_load(texture_path, &tex_width, &tex_height, &nrChannels, 0);
    if (!pixels) {
        SDL_LogCritical(0, "Failed to load texture: %s", texture_path);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (nrChannels == 4)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    else if (nrChannels == 3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(pixels);

    ////////////////////////////
    // Textured Quad
    glGenVertexArrays(1, &vao_quad);
    glBindVertexArray(vao_quad);

    glGenBuffers(1, &vbo_quad);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Quad::vertices), Quad::vertices, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &ibo_quad);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_quad);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Quad::indices), Quad::indices, GL_DYNAMIC_DRAW);


    glBindAttribLocation(default_shader_program._programId, 0, "vPos");
    glBindAttribLocation(default_shader_program._programId, 1, "vColor");
    glBindAttribLocation(default_shader_program._programId, 2, "vTexCoord");

    glEnableVertexAttribArray(vPos_attrib_location);
    glEnableVertexAttribArray(vColor_attrib_location);
    glEnableVertexAttribArray(vTexCoord_attrib_location);

    glVertexAttribPointer(vPos_attrib_location, 2, GL_FLOAT, false, sizeof(Vertex2D), (void *)offsetof(Vertex2D, Vertex2D::vPos));
    glVertexAttribPointer(vColor_attrib_location, 4, GL_FLOAT, false, sizeof(Vertex2D), (void *)offsetof(Vertex2D, Vertex2D::vColor));
    glVertexAttribPointer(vTexCoord_attrib_location, 2, GL_FLOAT, false, sizeof(Vertex2D), (void *)offsetof(Vertex2D, Vertex2D::vTexCoord));

    /////////////////////////
    /// Vertex2D Attributes


    quad_pos       = glm::vec2({ 100.f, 400.f });
    quad_transform = glm::mat4(1.0f);
    quad_transform = glm::translate(quad_transform, { quad_pos, 0.f });

    quad_transform = glm::translate(quad_transform, { 0.5f * tex_width, 0.5f * tex_height, 0.f });
    quad_transform = glm::rotate(quad_transform, glm::degrees(0.f), { 0.f, 0.f, 1.f });
    quad_transform = glm::translate(quad_transform, { -0.5f * tex_width, -0.5f * tex_height, 0.f });
    quad_transform = glm::scale(quad_transform, { tex_width, tex_height, 1.0f });

    quad_pos2       = glm::vec2({ 500, 100 });
    quad_transform2 = glm::translate(quad_transform2, { quad_pos2.x, quad_pos2.y, 0.f });
    quad_transform2 = glm::scale(quad_transform2, { 100.0f, 400.0f, 1.0f });

    camera_position = { 0.f, 0.f, 0.f };

    view = glm::mat4(1.0f);
    view = glm::translate(view, camera_position);

    // projection = glm::mat4(1.0f);
    // projection = glm::perspective(45.f, (float)(WND_WIDTH / WND_HEIGHT), .1f, 100.f);

    projection  = glm::ortho(0.f, (float)WND_WIDTH, 0.f, (float)WND_HEIGHT, .1f, 10.f);
    glm::mat4 V = glm::lookAt(glm::vec3(0.f, 0.f, 2.0f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.0f, 1.0f, 0.0f));

    projection = projection * V;

    default_shader_program.SetUniformMatrix4fvByName("model", quad_transform);
    default_shader_program.SetUniformMatrix4fvByName("view", view);
    default_shader_program.SetUniformMatrix4fvByName("projection", projection);
}

void GLUpdateAndRender(float dt)
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT /*| GL_DEPTH_BUFFER_BIT*/);

    default_shader_program.UseProgram();
    default_shader_program.SetUniformMatrix4fvByName("view", view);



    float x = 0, y = 0, z = 0;
    if (controls.up) y += 250.f * dt;
    if (controls.down) y -= 250.f * dt;
    if (controls.left) x -= 250.f * dt;
    if (controls.right) x += 250.f * dt;

    {
        glm::mat4 model = glm::mat4(1.0f);
        model           = glm::translate(model, { quad_pos.x += x, quad_pos.y += y, 0.f });

        model = glm::translate(model, { 0.5f * tex_width, 0.5f * tex_height, 0.f });
        model = glm::rotate(model, glm::degrees(0.f), { 0.f, 0.f, 1.f });
        model = glm::translate(model, { -0.5f * tex_width, -0.5f * tex_height, 0.f });

        model = glm::scale(model, { tex_width, tex_height, 1.0f });

        default_shader_program.SetUniformMatrix4fvByName("model", model);
    }

    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao_quad);
        glDrawElements(GL_TRIANGLES, sizeof(Quad::indices) / sizeof(Quad::indices[0]), GL_UNSIGNED_INT, 0);
    }
}