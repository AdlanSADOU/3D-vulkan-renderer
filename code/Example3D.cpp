#include "adgl.h"
#include "shader.h"

extern u32 WND_WIDTH;
extern u32 WND_HEIGHT;


float vertices[] = {
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.0f, 0.5f, -0.5f
};

unsigned short indices[] = {
    0, 1, 2
};

struct Mesh
{
    u32 vao;
    u32 vbo;
    u32 ibo;

    glm::vec3 position;
};

Mesh          triangle;
Shader        vertexShader;
Shader        fragShader;
ShaderProgram shaderProg;

void Example3DInit()
{
    vertexShader.CreateAndCompile("./shaders/3DVertexShader.vert", GL_VERTEX_SHADER);
    fragShader.CreateAndCompile("./shaders/fragmentShader.frag", GL_FRAGMENT_SHADER);
    shaderProg.AddShader(&vertexShader);
    shaderProg.AddShader(&fragShader);

    if (!shaderProg.CreateAndLinkProgram()) {
        SDL_Log("shader prog failed\n");
        exit(1);
    }
    shaderProg.UseProgram();

    glGenVertexArrays(1, &triangle.vao);
    glGenBuffers(1, &triangle.vbo);
    // glGenBuffers(1, &triangle.ibo);

    glGenBuffers(1, &triangle.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices) /*bytes*/, indices, GL_STATIC_DRAW);

    glBindVertexArray(triangle.vao);
    glBindBuffer(GL_ARRAY_BUFFER, triangle.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) /*bytes*/, vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0 /*layout location*/,
        3 /*number of components*/,
        GL_FLOAT /*type of components*/,
        GL_FALSE,
        sizeof(float) * 3,
        (void *)0);
    glEnableVertexAttribArray(0);

    Matrix4 projection = Matrix4::Perspective(.01f, 100.f, 45.f, ((float)WND_HEIGHT/(float)WND_WIDTH));
    glm::mat4 projectionGLM = glm::perspective(45.f, (float)WND_HEIGHT/(float)WND_WIDTH, .01f, 100.f);
    shaderProg.SetUniformMatrix4Name("projection", projection);
    shaderProg.SetUniformMatrix4Name("projectionGLM", projectionGLM);

    // glm::mat4 transform = glm::translate(glm::mat4(1), glm::vec3(.0f, .2f, .0f));
    // glm::vec4 position = {1.f,1.f,1.f, 1.f};
    // SDL_Log("(%f, %f, %f)\n", position.x, position.y, position.z);


    // Matrix44 transform;
    // Vector3  position = { 1, 1, 1 };
    // Vector3  a { 1, 2, 3 };
    // position = a;
    // position = position + a;

    // SDL_Log("(%f, %f, %f)\n", position.x, position.y, position.z);

    // // transform.Translate(position);
    // // shaderProg.SetUniformMatrix4Name("transform", )

    // Vector2 v {800, 600};
    // SDL_Log("V2 (%f, %f)\n", v.x, v.y);
    // SDL_Log("V2 (%f, %f)\n", v.u, v.v);
    // SDL_Log("V2 (%f, %f)\n", v.Width, v.Height);
}

Vector3 pos { 0, 0, 0 };
void    Example3DUpdateDraw(float dt, Input input)
{
    glClearColor(0.5f, 0.1f, 0.5f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shaderProg.UseProgram();
    if (input.right) {
        pos.x += .001f;
    }
    if (input.left) {
        pos.x += -.001f;
    }
    if (input.Q) {
        pos.y += .001f;
    }
    if (input.E) {
        pos.y += -.001f;
    }
    if (input.up) {
        pos.z += -.001f;
    }
    if (input.down) {
        pos.z += .001f;
    }

    // glm::mat4 transform = glm::mat4(1);
    // transform =  glm::translate(glm::mat4(1), glm::vec3(0,0,0));

    Matrix4 transform = Matrix4::Identity();
    transform = Matrix4::Translate(pos);
    shaderProg.SetUniformMatrix4Name("transform", transform);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle.ibo);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void *)0);
}