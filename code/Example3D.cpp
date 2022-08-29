/*TODO
    Adding materials to Model: the way we are currently handling this
    is quite... meh. Dont pre-size the Model._submeshMaterials
    it's too error prone.
    Instead do Model._submeshMaterials.push_back()

    [Suggestions]:
        - indexed assets
        - indexed materials(load shader once and use multiple instances)
            - need hashtable
                - learn big O notation
        - batched meshes


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

    - "each joint node may have a local tranform and an array of children"
    - "the bones of the skeleton are given implicitly, as the connections between the joints"

    - JOINTS_0 & WEIGHTS_0 refer to an accessor
        JOINTS_0 contains the indices of the joints affecting the vertex: 8bit vec4(j0, j1, j2, j3)
        WIGHTS_0 defines the weights: vec4(w0, w1, w2, w3)
        from these informations the "Skinning Matrix" can be computed
            see "Computing the skinning matrix"

    - reading:
        - https://gamemath.com/book/multiplespaces.html
        - https://computergraphics.stackexchange.com/questions/7603/confusion-about-how-inverse-bind-pose-is-actually-calculated-and-used#:~:text=The%20inverse%20bind%20pose%2C%20which,*%20v%2C%20which%20makes%20sense.
        - https://www.khronos.org/opengl/wiki/Skeletal_Animation
        - https://www.freecodecamp.org/news/advanced-opengl-animation-technique-skeletal-animations/
        - https://moddb.fandom.com/wiki/OpenGL:Tutorials:Basic_Bones_System#How_does_a_bone_system_work?
*/


// https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_004_ScenesNodes.md
// https://kcoley.github.io/glTF/specification/2.0/figures/gltfOverview-2.0.0a.png
// https://www.khronos.org/files/gltf20-reference-guide.pdf
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
#define CGLTF_IMPLEMENTATION
#include "adgl.h"

// todo: here we've included Mesh.h before Animation.h because Mesh.h
// includes cgltf.h. if we also include it in Animtion.h then compiler
// complains about multiple cgltf includes
// we might want to centralise all code that deals with cgltf in a single source file
#include "Mesh.h"
#include "Animation.h"

#include "Camera.h"
#include <vector>
#include <unordered_map>

// todo(ad): should these be provided as globals?
// how should we deal with general window parameters?
extern u32 WND_WIDTH;
extern u32 WND_HEIGHT;

static Camera camera = {};

/** note(ad):
 * we might want to find a proper way to handle
 * assets down the road if this becomes a problem
*/
std::unordered_map<std::string, Texture *>  gTextures;
std::unordered_map<std::string, Material *> gMaterials;

struct Ground
{
    Material *material;
    uint32_t  VAO;
} ground;

struct Entity
{
    Model     model;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

Mesh cubeMesh {};

Entity entities[3] {};

struct Point
{
    float     position[3];
    uint32_t  VAO;
    uint32_t  VBO;
    Material *material;
};
Point point {};


void Example3DInit()
{
    CameraCreate(&camera, { 0, 5, 16 }, 45.f, WND_WIDTH / (float)WND_HEIGHT, .8f, 1000.f);
    gCameraInUse = &camera;

    gTextures.insert({ "brick_wall", TextureCreate("assets/brick_wall.jpg") });
    gTextures.insert({ "cs_blend_red Base Color", TextureCreate("assets/cs_blend_red Base Color.png") });
    gTextures.insert({ "ground", TextureCreate("assets/ground.jpg") });
    gTextures.insert({ "cube", TextureCreate("assets/cube.png") });
    gTextures.insert({ "Material-1", TextureCreate("assets/Material-1.png") });
    gTextures.insert({ "Material-2", TextureCreate("assets/Material-2.png") });
    gTextures.insert({ "Material", TextureCreate("assets/Material.png") });


    gMaterials.insert({ "standard", MaterialCreate("shaders/point.shader", NULL) });
    gMaterials.insert({ "mileMaterial", MaterialCreate("shaders/standard.shader", gTextures["brick_wall"]) });
    gMaterials.insert({ "cubeMaterial", MaterialCreate("shaders/standard.shader", gTextures["cube"]) });
    gMaterials.insert({ "chibiMaterial", MaterialCreate("shaders/standard.shader", gTextures["cs_blend_red Base Color"]) });
    gMaterials.insert({ "groundMaterial", MaterialCreate("shaders/ground.shader", gTextures["ground"]) });



    PrintAnimationClipTransforms();


    std::string lowPoly01       = "low_poly_01.gltf";
    std::string char05Milestone = "char_05_milestone.gltf";

    std::string path = "assets/" + char05Milestone;

    entities[0].model.Create(path.c_str());
    entities[0].model._meshes[0]._submeshMaterials[0] = gMaterials["mileMaterial"];
    entities[0].model._meshes[0]._submeshMaterials[1] = gMaterials["mileMaterial"];
    entities[0].model._meshes[0]._submeshMaterials[2] = gMaterials["mileMaterial"];
    entities[0].model._transform                      = glm::mat4(1);
    entities[0].position                              = { 0.f, 0.f, -6.f };
    entities[0].rotation                              = { 0.f, 0.f, 0.f };
    entities[0].scale                                 = { .05, .05, .05 };

    entities[1].model.Create("assets/skinned_cube.gltf");
    entities[1].model._meshes[0]._submeshMaterials[0] = gMaterials["cubeMaterial"];
    entities[1].model._transform                      = glm::mat4(1);
    entities[1].position                              = { -4.f, 0, -6.f };
    entities[1].rotation                              = { 0, 0, 0 };
    entities[1].scale                                 = { 1, 1, 1 };


    entities[2].model.Create("assets/chibi_02_ex.gltf");
    entities[2].model._meshes[0]._submeshMaterials[0] = gMaterials["chibiMaterial"];
    // entities[2].model._meshes[1]._submeshMaterials[0] = &chibiMaterial;
    entities[2].model._transform = glm::mat4(1);
    entities[2].position         = { 4.f, 0.f, -6.f };
    entities[2].rotation         = { 0.f, 0.f, 0.f };
    entities[2].scale            = { 0.05f, 0.05f, 0.05f };


    ground.material = gMaterials["groundMaterial"];
    glGenVertexArrays(1, &ground.VAO);

    point = {
        .position = { 0.f, 0.f, 0.f },
        .material = gMaterials["standard"]
    };

    glGenVertexArrays(1, &point.VAO);
    glBindVertexArray(point.VAO);

    glGenBuffers(1, &point.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, point.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(point.position), point.position, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
}

float speed       = 20.f;
float sensitivity = 0.46f;

void DrawPoint(Point &p, glm::vec4 color)
{
    ShaderUse(p.material->_shader->programID);
    glm::mat4 model = glm::mat4(1);
    model           = glm::translate(model, { 1, 1, 1 });
    ShaderSetMat4ByName("projection", gCameraInUse->projection, p.material->_shader->programID);
    ShaderSetMat4ByName("view", gCameraInUse->view, p.material->_shader->programID);
    ShaderSetMat4ByName("model", model, p.material->_shader->programID);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glPointSize(60.f);
    glBindVertexArray(p.VAO);
    glDrawArrays(GL_POINTS, 0, 1);
}

void Example3DUpdateDraw(float dt, Input input)
{
    glm::vec3 directionDelta { 0, 0, 0 };

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
        camera.position -= speed * glm::normalize(glm::cross(camera.forward, camera.up)) * dt;
    }
    if (input.right) {
        camera.position += speed * glm::normalize(glm::cross(camera.forward, camera.up)) * dt;
    }

    float factor = 0;
    if (input.E) factor += .15f;
    if (input.Q) factor -= .15f;
    camera.position.y += factor;

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


    camera.forward.x = cosf(Radians(camera.yaw)) * cosf(Radians(camera.pitch));
    camera.forward.y = sinf(Radians(camera.pitch));
    camera.forward.z = sinf(Radians(camera.yaw)) * cosf(Radians(camera.pitch));
    camera.forward   = glm::normalize(camera.forward);
    xrelPrev         = xrel;
    yrelPrev         = yrel;

    gCameraInUse->aspect = WND_WIDTH / (float)WND_HEIGHT;
    CameraUpdate();
    static float modelYaw = 0;
    modelYaw += 1.f;
    entities[0].rotation.y = modelYaw;

    /**
     * DrawPoint(p1, color);
     * DrawLine(p1, p2, color);
     */





    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // todo(): simplify to ShaderUse(material->_shader)

    DrawPoint(point, { 1, 1, 1, .1f });

    ShaderUse(ground.material->_shader->programID);
    ShaderSetMat4ByName("proj", gCameraInUse->projection, ground.material->_shader->programID);
    ShaderSetMat4ByName("view", gCameraInUse->view, ground.material->_shader->programID);
    glBindTexture(GL_TEXTURE_2D, ground.material->_texture->id);
    glBindVertexArray(ground.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);



    for (size_t n = 0; n < ARR_COUNT(entities); n++) {
        for (int i = 0; i < entities[n].model._meshCount; i++) {
            glm::mat4 model = glm::mat4(1);

            model = glm::translate(model, entities[n].position)
                * glm::rotate(model, (Radians(entities[n].rotation.x)), glm::vec3(1, 0, 0))
                * glm::rotate(model, (Radians(entities[n].rotation.y)), glm::vec3(0, 1, 0))
                * glm::rotate(model, (Radians(entities[n].rotation.z)), glm::vec3(0, 0, 1))
                * glm::scale(model, entities[n].scale)
                * entities[n].model._transform;

            for (int j = 0; j < entities[n].model._meshes[i]._submeshCount; j++) {
                Material *material = entities[n].model._meshes[i]._submeshMaterials[j];
                ShaderUse(material->_shader->programID);
                ShaderSetMat4ByName("projection", gCameraInUse->projection, material->_shader->programID);
                ShaderSetMat4ByName("view", gCameraInUse->view, material->_shader->programID);
                ShaderSetMat4ByName("model", model, material->_shader->programID);
                glBindTexture(GL_TEXTURE_2D, material->_texture->id);

                Mesh mesh = entities[n].model._meshes[i];
                glBindVertexArray(mesh._VAOs[j]);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh._IBOs[j]);
                glDrawElements(GL_TRIANGLES, mesh._submeshIndices[j].size(), GL_UNSIGNED_SHORT, (void *)0);
            }
        }
    }
}