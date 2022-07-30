#include "adgl.h"
#include "shader.h"

#include "stb_image.h"

extern u32 WND_WIDTH;
extern u32 WND_HEIGHT;

static void printVec(const char *name, Vector3 v)
{
    SDL_Log("%s: (%f, %f, %f)\n", name, v.x, v.y, v.z);
}

static float triangle_verts[] = {
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.0f, 0.5f, -0.5f
};

static float cube_verts[] = {
    // front
    -1.0, -1.0, 1.0, 0.0, 0.0,
    1.0, -1.0, 1.0, 1.0, 0.0,
    1.0, 1.0, 1.0, 1.0, 1.0,
    -1.0, 1.0, 1.0, 0.0, 1.0,
    // back
    -1.0, -1.0, -1.0, 0.0, 0.0,
    1.0, -1.0, -1.0, 1.0, 0.0,
    1.0, 1.0, -1.0, 1.0, 1.0,
    -1.0, 1.0, -1.0, 0.0, 1.0
};

static u16 cube_indices[] = {
    // front
    0, 1, 2,
    2, 3, 0,
    // right
    1, 5, 6,
    6, 2, 1,
    // back
    7, 6, 5,
    5, 4, 7,
    // left
    4, 0, 3,
    3, 7, 4,
    // bottom
    4, 5, 1,
    1, 0, 4,
    // top
    3, 2, 6,
    6, 7, 3
};

static u16 triangle_indices[] = {
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

static Mesh cube;

// material
static Shader        vertexShader;
static Shader        fragShader;
static ShaderProgram shaderProg;

static GLuint texture;
static int    tex_width, tex_height, nrChannels;

//camera
struct Camera
{
    Vector3 position;
    Vector3 up;
    Vector3 right;
    Vector3 forward;
    float   pitch;
    float   yaw;
    float   roll;
};
static Camera camera = {};


void Example3DInit()
{
    glGenVertexArrays(1, &cube.vao);
    glBindVertexArray(cube.vao);
    // glGenBuffers(1, &cube.ibo);

    glGenBuffers(1, &cube.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, cube.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verts) /*bytes*/, cube_verts, GL_STATIC_DRAW);

    glGenBuffers(1, &cube.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices) /*bytes*/, cube_indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0 /*layout location*/,
        3 /*number of components*/,
        GL_FLOAT /*type of components*/,
        GL_FALSE,
        sizeof(float) * 5,
        (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1 /*layout location*/,
        2 /*number of components*/,
        GL_FLOAT /*type of components*/,
        GL_FALSE,
        sizeof(float) * 5,
        (void *)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);




    //////////////////////////////////////
    /// Texture
    char texture_path[] = "assets/brick_wall.jpg";

    unsigned char *pixels = stbi_load(texture_path, &tex_width, &tex_height, &nrChannels, 0);
    if (!pixels) {
        SDL_LogCritical(0, "Failed to load texture: %s", texture_path);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (nrChannels == 4)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    else if (nrChannels == 3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(pixels);
    glBindTexture(GL_TEXTURE_2D, 0);




    // Shader setup
    vertexShader.CreateAndCompile("./shaders/3DVertexShader.vert", GL_VERTEX_SHADER);
    fragShader.CreateAndCompile("./shaders/fragmentShader.frag", GL_FRAGMENT_SHADER);
    shaderProg.AddShader(&vertexShader);
    shaderProg.AddShader(&fragShader);

    if (!shaderProg.CreateAndLinkProgram()) {
        SDL_Log("shader prog failed\n");
        exit(1);
    }
    shaderProg.UseProgram();




    // Matrices setup
    Matrix4 projection = Matrix4::Perspective(.01f, 1000.f, 45.f, ((float)WND_HEIGHT / (float)WND_WIDTH));
    shaderProg.SetUniformMatrix4Name("projection", projection);


    cube.transform = {};
    cube.position  = { 0.f, 0.f, -6.f };
    cube.transform = Matrix4::Translate(cube.position);
    shaderProg.SetUniformMatrix4Name("model", cube.transform);


    camera.position = { 0, 0, 16 };
    camera.forward  = { 0, 0, -1 };
    camera.up       = { 0, 1, 0 };
    camera.yaw      = -90;
    camera.pitch    = 0;

    // glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    // shaderProg.SetUniformMatrix4Name("view", view);
}

float speed       = 5.f;
float sensitivity = 0.16f;
float fov         = 45;
void  Example3DUpdateDraw(float dt, Input input)
{
    glClearColor(0.3f, 0.3f, 0.3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shaderProg.UseProgram();

    Vector3 directionDelta { 0, 0, 0 };

#if 0
    if (input.right) {
        directionDelta.x = .01f;
    }
    if (input.left) {
        directionDelta.x = -.01f;
    }
    if (input.Q) {
        directionDelta.y = .01f;
    }
    if (input.E) {
        directionDelta.y = -.01f;
    }
    if (input.up) {
        directionDelta.z = -.01f;
    }
    if (input.down) {
        directionDelta.z = .01f;
    }
#endif

    if (input.up) {
        camera.position += speed * camera.forward * dt;
    }
    if (input.down) {
        camera.position -= speed * camera.forward * dt;
    }
    if (input.left) {
        camera.position -= speed * Vector3::Normalize(Vector3::Cross(camera.forward, camera.up)) * dt;
    }
    if (input.right) {
        camera.position += speed * Vector3::Normalize(Vector3::Cross(camera.forward, camera.up)) * dt;
    }
    if (input.E) fov += .1f;
    if (input.Q) fov -= .1f;



    cube.transform = Matrix4::Translate(cube.position);
    // shaderProg.SetUniformMatrix4Name("model", cube.transform);

    static float xrelPrev = 0;
    static float yrelPrev = 0;
    int          xrel;
    int          yrel;

    SDL_GetRelativeMouseState(&xrel, &yrel);
    if (input.mouse.right) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        // printVec("rel", {(float)xrel, (float)yrel, 0});

        if (xrelPrev != xrel)
            camera.yaw
                += xrel * .1f * sensitivity;
        if (xrelPrev != xrel)
            camera.pitch += yrel * .008f * sensitivity;
    } else
        SDL_SetRelativeMouseMode(SDL_FALSE);

    camera.forward.x  = cos(Radians(camera.yaw)) * cos(camera.pitch);
    camera.forward.y  = sin(camera.pitch);
    camera.forward.z  = sin(Radians(camera.yaw)) * cos(camera.pitch);
    camera.forward    = camera.forward.Normalized();
    xrelPrev          = xrel;
    yrelPrev          = yrel;
    camera.position.y = 0;

    // printVec("camPos", camera.position);
    // camera.position += directionDelta;

    // Matrix4 projection = Matrix4::Perspective(.1f, 100.f, fov, ((float)WND_HEIGHT / (float)WND_WIDTH));
    // shaderProg.SetUniformMatrix4Name("projection", projection);

    Vector3 at   = camera.position + camera.forward;
    Matrix4 view = LookAt(camera.position, at, camera.up);
    shaderProg.SetUniformMatrix4Name("view", view);

    // glm::mat4 projection = glm::perspective(fov, ((float)WND_HEIGHT / (float)WND_WIDTH), .1f, 100.f);
    // shaderProg.SetUniformMatrix4Name("projection", projection);
    // glm::vec3 at;
    // at.x = camera.position.x + camera.forward.x;
    // at.y = camera.position.y + camera.forward.y;
    // at.z = camera.position.z + camera.forward.z;
    // glm::mat4 view = glm::lookAt(glm::vec3(camera.position.x, camera.position.y, camera.position.z), glm::vec3(at.x, at.y, at.z), glm::vec3(0, 1, 0));
    // shaderProg.SetUniformMatrix4Name("view", view);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube.ibo);
    glBindVertexArray(cube.vao);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawElements(GL_TRIANGLES, sizeof(cube_indices) / sizeof(cube_indices[0]), GL_UNSIGNED_SHORT, (void *)0);
}