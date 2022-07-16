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

    Vector3 position;
    Matrix4 transform;
};

Mesh          triangle;
Shader        vertexShader;
Shader        fragShader;
ShaderProgram shaderProg;

//camera
struct Camera
{
    Vector3 position;
    Vector3 direction;
    Vector3 up;
    Vector3 right;
};


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

    Matrix4 projection = Matrix4::Perspective(.01f, 100.f, 45.f, ((float)WND_HEIGHT / (float)WND_WIDTH));
    shaderProg.SetUniformMatrix4Name("projection", projection);

    triangle.transform = {};
    triangle.position  = { 0.f, 0.f, -3.f };

    triangle.transform = Matrix4::Translate(triangle.position);
    shaderProg.SetUniformMatrix4Name("transform", triangle.transform);


    //camera
    Camera camera;

    SDL_Log("%f, %f, %f\n", camera.right.x, camera.right.y, camera.right.z);
    SDL_Log("%f, %f, %f\n", camera.up.x, camera.up.y, camera.up.z);
    SDL_Log("%f, %f, %f\n", camera.direction.x, camera.direction.y, camera.direction.z);
}


void Example3DUpdateDraw(float dt, Input input)
{
    glClearColor(0.5f, 0.1f, 0.5f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shaderProg.UseProgram();

    Vector3 directionDelta { 0, 0, 0 };
    if (input.right) {
        directionDelta.x = .001f;
    }
    if (input.left) {
        directionDelta.x = -.001f;
    }
    if (input.Q) {
        directionDelta.y = .001f;
    }
    if (input.E) {
        directionDelta.y = -.001f;
    }
    if (input.up) {
        directionDelta.z = -.001f;
    }
    if (input.down) {
        directionDelta.z = .001f;
    }

    triangle.position += directionDelta;

    triangle.transform = Matrix4::Translate(triangle.position);
    shaderProg.SetUniformMatrix4Name("transform", triangle.transform);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle.ibo);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void *)0);
}