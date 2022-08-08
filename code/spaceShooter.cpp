#include "adgl.h"
#include "shader.h"

extern u32   WND_WIDTH;
extern u32   WND_HEIGHT;
static float aspect = (float)WND_WIDTH / (float)WND_HEIGHT;

float triangle_verts[] = {
    -1.f, -1.f, -1.f, .1f, .6f, .1f, 1.f,
    1.f, -1.f, -1.f, .1f, .6f, .1f, 1.f,
    0.f, 1.f, -1.f, .1f, .6f, .1f, 1.f
};

struct Triangle
{
    uint32_t VAO;
    uint32_t VBO;
    uint32_t IBO;
};

Triangle triangle;

struct Entity
{
    Vector3 position;
    Vector3 forward;
    Vector3 right;
    Matrix4 localToWorld;
};

static const u32 MAX_ENTITIES = 10;
static Entity    entities[MAX_ENTITIES];

static Shader        vertexShader;
static Shader        fragmentShader;
static ShaderProgram shaderProg;

void InitSpaceShooter()
{
    SDL_SetRelativeMouseMode(SDL_TRUE);

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





    Matrix4 projection = Perspective(.01f, 100.f, 40.f, aspect);
    shaderProg.SetUniformMatrix4Name("projection", projection);


    entities[0].position     = { 0, 0, 0 };
    entities[0].localToWorld = RotateX(Radians(90));


    entities[1].position     = { 2, 0, -3 };
    entities[1].localToWorld = Identity();
    entities[2].position     = { -2, 0, -3 };
    entities[2].localToWorld = Identity();
}



static float sensitivity = 400.f;
static float playerSpeed = 15;
static float pitch       = 0;
static float yaw         = 90;


void UpdateDrawSpaceShooter(float dt, Input input)
{
    Vector3 direction = {};

    // if (input.up) direction.z -= .1;
    // if (input.down) direction.z += .1;
    if (input.left) direction.x -= .1;
    if (input.right) direction.x += .1;
    if (input.Q) direction.y += .1f;
    if (input.E) direction.y -= .1f;

    static float prevXrel = 0;
    static float prevYrel = 0;

    SDL_GetRelativeMouseState(&input.mouse.xrel, &input.mouse.yrel);
    if (input.mouse.xrel != prevXrel) {
        yaw += (input.mouse.xrel / (float)WND_WIDTH) * sensitivity;
        // SDL_Log("%f\n", yaw);
    }
    // if (input.mouse.yrel != prevYrel)

    prevXrel = input.mouse.xrel;
    prevYrel = input.mouse.yrel;



    shaderProg.UseProgram();

    Vector3 forward;
    forward.x = cosf(Radians(yaw));
    forward.y = 0;
    forward.z = sinf(Radians(yaw));

    Vector3 right = Vector3::Cross(forward, { 0, 1, 0 });


    if (input.up)
        entities[0].position -= forward.Normalized() * dt * playerSpeed;
    if (input.down)
        entities[0].position += forward.Normalized() * dt * playerSpeed;
    if (input.left)
        entities[0].position += right.Normalized() * dt * playerSpeed;
    if (input.right)
        entities[0].position -= right.Normalized() * dt * playerSpeed;

    Matrix4 view = LookAt(entities[0].position + forward.Normalized() * 10, entities[0].position + Vector3(0, 0, 0), { 0, 1, 0 });
    shaderProg.SetUniformMatrix4Name("view", view);

    glBindVertexArray(triangle.VAO);

    Matrix4 model = entities[0].localToWorld * RotateY(Radians(yaw) - PI / 2) * Translate(entities[0].position);
    shaderProg.SetUniformMatrix4Name("model", model);
    glDrawArrays(GL_LINE_LOOP, 0, 3);

    for (size_t i = 1; i < MAX_ENTITIES; i++) {
        Matrix4 model = entities[i].localToWorld * Translate(entities[i].position);
        shaderProg.SetUniformMatrix4Name("model", model);
        glDrawArrays(GL_LINE_LOOP, 0, 3);
    }
}