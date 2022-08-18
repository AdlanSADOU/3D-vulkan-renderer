/*TODO
    Adding materials to Model: the way we are currently handling this
    is quite... meh. Dont pre-size the Model.submeshMaterials
    it's too error prone.
    Instead do Model.submeshMaterials.push_back()

    model loading
    texture management
    lighting
    3D animation

    Issues:
    - for some reason the cube texture is being used on the feet.
    Feet is mesh the first drawn mesh.
    One cause might be, as the cube is being drawn last, than the last bound
    material is used on the next iteration, which are the feet.

    [checked: false assumption]
    changing draw order does not affect the issue

    [solved] the issue was that the original texture for feet has been overwritten
    by the texture for the sword

    [solution] make blender output texture names prefixed with project name... or something


    - how to find the root joint? :
        the root joint is the last element in the data->nodes[] array

    - each joint has a matrix called inverseBindMatrix
        what's its purpose?
            transforms vertices into the local space of the joint

    - "each joint node may a local tranform and an array of children"
    - "the bones of the skeleton are given implicitly, as the connections between the joints"

    - JOINTS_0 & WEIGHTS_0 refer to an accessor
        JOINTS_0 contains the indices of the joints affecting the vertex
        WIGHTS_0 defines the weights
        from these informations the "Skinning Matrix" can be computed
            see "Computing the skinning matrix"

    - reading:
        - https://gamemath.com/book/multiplespaces.html
        - https://computergraphics.stackexchange.com/questions/7603/confusion-about-how-inverse-bind-pose-is-actually-calculated-and-used#:~:text=The%20inverse%20bind%20pose%2C%20which,*%20v%2C%20which%20makes%20sense.
        - https://www.khronos.org/opengl/wiki/Skeletal_Animation
        - https://www.freecodecamp.org/news/advanced-opengl-animation-technique-skeletal-animations/


*/


// https://kcoley.github.io/glTF/specification/2.0/figures/gltfOverview-2.0.0a.png
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
#define CGLTF_IMPLEMENTATION
#include "adgl.h"
#include "Mesh.h"
#include "Camera.h"
#include <HandmadeMath.h>

extern u32 WND_WIDTH;
extern u32 WND_HEIGHT;

static GLuint texture;
static int    tex_width, tex_height, nrChannels;

static Camera camera = {};

// static Model    milechar;
static Material mileMaterial_1;
static Material mileMaterial_2;
static Material mileMaterial_3;
static Material cubeMaterial;
static Material chibiMaterial;

static Texture mileTexture_1;
static Texture mileTexture_2;
static Texture mileTexture_3;
static Texture cubeTexture;
static Texture chibiTexture;

static const char *vertexShaderPath   = "./shaders/3DVertexShader.vert";
static const char *fragmentShaderPath = "./shaders/fragmentShader.frag";

struct Entity
{
    Model   model;
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
};

Mesh cubeMesh {};

Entity entities[3] {};

void Example3DInit()
{
    const char *  skeletonPath = "assets/skeleton.gltf";
    cgltf_options opts {};
    cgltf_data *  data {};
    cgltf_result  res = cgltf_parse_file(&opts, skeletonPath, &data);
    if (res == cgltf_result_success) {
        res = cgltf_load_buffers(&opts, data, skeletonPath);
        if (res == cgltf_result_success) {
            SDL_Log(data->nodes[0].name);
            SDL_Log("count: %d", data->nodes[0].parent->children_count);
        }
    }



    std::string lowPoly01       = "low_poly_01.gltf";
    std::string char05Milestone = "char_05_milestone.gltf";

    std::string path = "assets/" + char05Milestone;

    mileTexture_1.Create("assets/material.png");
    mileTexture_2.Create("assets/material-1.png");
    mileTexture_3.Create("assets/material-2.png");

    mileMaterial_1.Create(vertexShaderPath, fragmentShaderPath, &mileTexture_1);
    mileMaterial_2.Create(vertexShaderPath, fragmentShaderPath, &mileTexture_2);
    mileMaterial_3.Create(vertexShaderPath, fragmentShaderPath, &mileTexture_3);

    entities[0].model.Create(path.c_str());
    entities[0].model._meshes[0].submeshMaterials[0] = &mileMaterial_1;
    entities[0].model._meshes[0].submeshMaterials[1] = &mileMaterial_2;
    entities[0].model._meshes[0].submeshMaterials[2] = &mileMaterial_3;
    entities[0].model._transform                     = Identity();
    entities[0].position                             = { 0.f, 0.f, -6.f };
    entities[0].rotation                             = { 0.f, 0.f, 0.f };
    entities[0].scale                                = { .05, .05, .05 };


    // cubeMesh.submeshCount = 1;
    // cubeMesh.submeshVertices.push_back(cube_verts);
    // cubeMesh.submeshIndices.push_back(cube_indices);
    // cubeMesh.Create();
    cubeTexture.Create("assets/cube.png");
    cubeMaterial.Create(vertexShaderPath, fragmentShaderPath, &cubeTexture);

    entities[1].model.Create("assets/cube.gltf");
    entities[1].model._meshes[0].submeshMaterials[0] = (&cubeMaterial);
    entities[1].model._transform                     = Identity();
    entities[1].position                             = { -4.f, 0, -6.f };
    entities[1].scale                                = { 1, 1, 1 };
    entities[1].rotation                             = { 0, -90, 0 };


    // chibiTexture.Create("assets/lambert1 Base Color.png");
    // chibiMaterial.Create(vertexShaderPath, fragmentShaderPath, &chibiTexture);

    // entities[2].model.Create("assets/chibi_02_ex.gltf");
    // entities[2].model._meshes[0].submeshMaterials[0] = &chibiMaterial;
    // entities[2].model._transform                     = Identity();
    // entities[2].position                             = { 4.f, 0.f, -6.f };
    // entities[2].rotation                             = { 0.f, 0.f, 0.f };
    // entities[2].scale                                = { .05, .05, .05 };

    camera.position = { 0, 5, 16 };
    camera.forward  = { 0, 0, -1 };
    camera.up       = { 0, 1, 0 };
    camera.yaw      = -90;
    camera.pitch    = 0;
}

float speed       = 15.f;
float sensitivity = 0.46f;
float fov         = 60;

void Example3DUpdateDraw(float dt, Input input)
{

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

    static float factor = 0;
    if (input.E) factor += .01f;
    if (input.Q) factor -= .01f;

    static float xrelPrev = 0;
    static float yrelPrev = 0;
    int          xrel;
    int          yrel;

    SDL_GetRelativeMouseState(&xrel, &yrel);
    if (input.mouse.right) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        // printVec("rel", {(float)xrel, (float)yrel, 0});

        if (xrelPrev != xrel)
            camera.yaw += xrel * .1f * sensitivity;
        if (xrelPrev != xrel)
            camera.pitch -= yrel * .1f * sensitivity;
    } else
        SDL_SetRelativeMouseMode(SDL_FALSE);


    camera.forward.x = cos(Radians(camera.yaw)) * cos(Radians(camera.pitch));
    camera.forward.y = sin(Radians(camera.pitch));
    camera.forward.z = sin(Radians(camera.yaw)) * cos(Radians(camera.pitch));
    camera.forward   = camera.forward.Normalized();
    xrelPrev         = xrel;
    yrelPrev         = yrel;


    Vector3      at         = camera.position + camera.forward;
    Matrix4      projection = Perspective(.01f, 1000.f, fov, (float)WND_WIDTH / (float)WND_HEIGHT);
    Matrix4      view       = LookAt(camera.position, at, camera.up);
    static float modelYaw   = 0;
    modelYaw += 1.f;
    entities[0].rotation.y = modelYaw;

    /**
     * draw
     */

    for (size_t n = 0; n < ARR_COUNT(entities) - 1; n++) {
        for (int i = 0; i < entities[n].model._meshCount; i++) {
            Matrix4 model = Scale(entities[n].scale)
                * RotateZ(Radians(entities[n].rotation.z))
                * RotateY(Radians(entities[n].rotation.y))
                * RotateX(Radians(entities[n].rotation.x))
                * Translate(entities[n].position)
                * entities[n].model._transform;

            for (int j = 0; j < entities[n].model._meshes[i].submeshCount; j++) {
                entities[n].model._meshes[i].submeshMaterials[j]->_program.UseProgram();
                entities[n].model._meshes[i].submeshMaterials[j]->_program.SetUniformMatrix4Name("projection", projection);
                entities[n].model._meshes[i].submeshMaterials[j]->_program.SetUniformMatrix4Name("view", view);
                entities[n].model._meshes[i].submeshMaterials[j]->_program.SetUniformMatrix4Name("model", model);

                glBindVertexArray(entities[n].model._meshes[i].VAOs[j]);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entities[n].model._meshes[i].IBOs[j]);
                glBindTexture(GL_TEXTURE_2D, entities[n].model._meshes[i].submeshMaterials[j]->_texture->id);
                glDrawElements(GL_TRIANGLES, entities[n].model._meshes[i].submeshIndices[j].size(), GL_UNSIGNED_SHORT, (void *)0);
            }
        }
    }
}