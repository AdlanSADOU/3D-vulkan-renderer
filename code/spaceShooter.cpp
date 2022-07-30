#include "adgl.h"
#include "shader.h"

extern u32   WND_WIDTH;
extern u32   WND_HEIGHT;
static float aspect = (float)WND_WIDTH / (float)WND_HEIGHT;

float triangle_verts[] = {
    -0.5f, -0.5f, -0.5f, .1f, .6f, .1f, 1.f,
    0.5f, -0.5f, -0.5f, .1f, .6f, .1f, 1.f,
    0.0f, 0.5f, -0.5f, .1f, .6f, .1f, 1.f
};

struct Entity
{
    uint32_t VAO;
    uint32_t VBO;
    uint32_t IBO;

    Vector3 position;
};

static Entity        triangle;
static Shader        vertexShader;
static Shader        fragmentShader;
static ShaderProgram shaderProg;

void InitSpaceShooter()
{
    glGenVertexArrays(1, &triangle.VAO);
    glBindVertexArray(triangle.VAO);

    glGenBuffers(1, &triangle.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, triangle.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_verts), triangle_verts, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void *)(sizeof(float) * 3));





    vertexShader.CreateAndCompile("shaders/spaceShooter.vert", GL_VERTEX_SHADER);
    fragmentShader.CreateAndCompile("shaders/spaceShooter.frag", GL_FRAGMENT_SHADER);
    shaderProg.AddShader(&vertexShader);
    shaderProg.AddShader(&fragmentShader);
    if (!shaderProg.CreateAndLinkProgram()) SDL_Log("Shader Prog failed...");
    shaderProg.UseProgram();





    Matrix4 projection = Matrix4::Perspective(.01f, 100.f, 40.f, aspect);
    Matrix4 view       = LookAt({ 0, 0, 10 }, { 0, 0, 0 }, { 0, 1, 0 });
    Matrix4 model      = Matrix4::Identity();
    model              = Matrix4::Translate({ 0, 0, -2 });

    shaderProg.SetUniformMatrix4Name("projection", projection);
    shaderProg.SetUniformMatrix4Name("view", view);
    shaderProg.SetUniformMatrix4Name("model", model);
}

void UpdateDrawSpaceShooter(float dt, Input input)
{
    glClearColor(.2f, .2f, .2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shaderProg.UseProgram();
    glBindVertexArray(triangle.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}