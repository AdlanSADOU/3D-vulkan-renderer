/*TODO
    Player sprite & movement

*/
#include "shader.h"

#include <stdio.h>
#include <stdlib.h>

#include <adgl.h>
#include <Texture.h>

#define MATH_3D_IMPLEMENTATION
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Vertex2D
{
    Vector2   vPos;
    glm::vec4 vColor;
    Vector2   vTexCoord;
};

struct Quad
{
    static inline Vertex2D vertices[4] = {
        { { +0.5f, +0.5f }, { 1.0, 1.0, 1.0, 1.0 }, { 1, 1 } },
        { { +0.5f, -0.5f }, { 1.0, 1.0, 1.0, 1.0 }, { 1, 0 } },
        { { -0.5f, -0.5f }, { 1.0, 1.0, 1.0, 1.0 }, { 0, 0 } },
        { { -0.5f, +0.5f }, { 1.0, 1.0, 1.0, 1.0 }, { 0, 1 } },
    };

    static inline GLuint indices[6] = {
        0, 1, 3,
        1, 2, 3
    };
};

extern uint32_t WND_WIDTH;
extern uint32_t WND_HEIGHT;
extern Input    input;

/* Important note
* these are globals right now but they won't stay that way
* only until I figure out where they really belong and how they interact
*/
Shader        vert_shader;
Shader        frag_shader;
ShaderProgram default_shader_program;

glm::mat4 view;
glm::mat4 projection;

Vector3 camera_position;

GLuint vao_quad;
GLuint vbo_quad;
GLuint ibo_quad;

Quad    quad;
Matrix4 quad_transform;
Vector2 quad_pos = {};


static Texture *texture;

void ExampleSPriteInit()
{

    vert_shader.ShaderSourceLoadAndCompile("./shaders/vertexShader.vert", GL_VERTEX_SHADER);
    frag_shader.ShaderSourceLoadAndCompile("./shaders/fragmentShader.frag", GL_FRAGMENT_SHADER);
    default_shader_program.AddShader(vert_shader);
    default_shader_program.AddShader(frag_shader);
    default_shader_program.CreateAndLinkProgram();
    default_shader_program.ShaderUse();

    GLint vPos_attrib_location      = 0;
    GLint vColor_attrib_location    = 1;
    GLint vTexCoord_attrib_location = 2;



    //////////////////////////////////////
    /// Texture

    texture = TextureCreate("assets/powerup.png");




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
    quad_pos       = Vector2({ WND_WIDTH / 2.f, WND_HEIGHT / 2.f });
    quad_transform = Identity();
    quad_transform = Translate({ quad_pos.x, quad_pos.y, 0.f });

    view = glm::mat4(1.f);
    // view = glm::translate(view, camera_position);

    // projection = Matrix4(1.0f);
    // projection = glm::perspective(45.f, (float)(WND_WIDTH / WND_HEIGHT), .1f, 100.f);

    // default_shader_program.ShaderSetMat4ByName("model", quad_transform);
}




void ExampleSpriteUpdateDraw(float dt, Input input)
{
    default_shader_program.ShaderUse();

    projection = glm::ortho(0.f, (float)WND_WIDTH, 0.f, (float)WND_HEIGHT, .1f, 10.f);
    view       = glm::lookAt(glm::vec3(0.f, 0.f, 2.0f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.0f, 1.0f, 0.0f));
    default_shader_program.ShaderSetMat4ByName("view", view);
    default_shader_program.ShaderSetMat4ByName("projection", projection);


    static float theta = 0;
    float        x = 0, y = 0, z = 0;
    if (input.up) y += 250.f * dt;
    if (input.down) y -= 250.f * dt;
    if (input.left) x -= 250.f * dt;
    if (input.right) x += 250.f * dt;
    if (input.Q) theta += 1.f;
    if (input.E) theta -= 1.f;

    {
        Matrix4 model = Identity();

        model = model * Scale({ (float)texture->width, (float)texture->height, 1.0f })
            * RotateZ(Radians(theta))
            * Translate({ quad_pos.x += x, quad_pos.y += y, 0.f });
        default_shader_program.ShaderSetMat4ByName("model", model);
    }

    {
        glBindTexture(GL_TEXTURE_2D, texture->id);
        glBindVertexArray(vao_quad);
        glDrawElements(GL_TRIANGLES, sizeof(Quad::indices) / sizeof(Quad::indices[0]), GL_UNSIGNED_INT, 0);
    }
}