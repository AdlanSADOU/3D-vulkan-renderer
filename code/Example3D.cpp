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
#include "Mesh.h"
#include "Camera.h"
#include <vector>
#include <unordered_map>

extern u32 WND_WIDTH;
extern u32 WND_HEIGHT;

static GLuint texture;
static int    tex_width, tex_height, nrChannels;

static Camera camera = {};

/** note(ad):
 * we might want to find a proper way to handle
 * assets down the road if this becomes a problem
*/
std::unordered_map<std::string, Texture *>  gTextures;
std::unordered_map<std::string, Material *> gMaterials;

// static Model    milechar;
static Material *mileMaterial_1;
static Material *mileMaterial_2;
static Material *mileMaterial_3;
static Material *cubeMaterial;
static Material *chibiMaterial;

static const char *standardShaderPath = "./shaders/standard.vert";
static const char *fragmentShaderPath = "./shaders/standard.frag";

struct Joint
{
    char *    name;
    int8_t    parent;
    glm::mat4 invBindPose;
} * joints;

struct Skeleton
{
    uint32_t id;
    uint32_t jointCount;
    Joint *  joints;
};

struct AnimationPose
{
    float     scale;
    glm::quat rotation;
    glm::vec3 translation;
};

/**
 * Is a pose sample actually an offset relative to
 * the rest pose or an offset relative to the previous pose...?
 */
struct AnimationClip
{
    uint32_t      id;
    float         duration;
    AnimationPose poseSamples;
};

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
    gTextures.insert({ "brick_wall", TextureCreate("assets/brick_wall.jpg") });
    gTextures.insert({ "cs_blend_red Base Color", TextureCreate("assets/cs_blend_red Base Color.png") });
    gTextures.insert({ "ground", TextureCreate("assets/ground.jpg") });
    gTextures.insert({ "cube", TextureCreate("assets/cube.png") });
    gTextures.insert({ "Material-1", TextureCreate("assets/Material-1.png") });
    gTextures.insert({ "Material-2", TextureCreate("assets/Material-2.png") });
    gTextures.insert({ "Material", TextureCreate("assets/Material.png") });


    gMaterials.insert({ "standard", MaterialCreate("shaders/point.vert", "shaders/point.frag", NULL) });
    gMaterials.insert({ "mileMaterial", MaterialCreate(standardShaderPath, fragmentShaderPath, gTextures["brick_wall"]) });
    gMaterials.insert({ "cubeMaterial", MaterialCreate(standardShaderPath, fragmentShaderPath, gTextures["cube"]) });
    gMaterials.insert({ "chibiMaterial", MaterialCreate(standardShaderPath, fragmentShaderPath, gTextures["cs_blend_red Base Color"]) });
    gMaterials.insert({ "groundMaterial", MaterialCreate("shaders/ground.vert", "shaders/ground.frag", gTextures["ground"]) });


    const char *  skeletonPath = "assets/skinned_cube.gltf";
    cgltf_options opts {};
    cgltf_data *  data {};
    cgltf_result  res = cgltf_parse_file(&opts, skeletonPath, &data);
    if (res == cgltf_result_success) {
        res = cgltf_load_buffers(&opts, data, skeletonPath);
        if (res == cgltf_result_success) {
            SDL_Log(data->nodes[0].name);
            SDL_Log("count: %d", data->nodes[0].parent->children_count);

            SDL_Log("%s", data->meshes[0].primitives[0].attributes[3].name);
            // SDL_Log("%d", data->meshes[0].primitives[0].attributes[3].data->buffer_view->);

            // what does "count" mean for JOINTS_0 data ?
            // SDL_Log("%d", data->meshes[0].primitives[0].attributes[3].data->count);

            // Attribute type
            // SDL_Log("skin: %d", data->meshes[0].primitives[0].attributes[3].type == cgltf_attribute_type_joints);
        }
    }

    // joints.push_back({(char *)"root", 0, glm::vec3(0,0,0), glm::quat(0,0,0,1), glm::vec3(1,1,1), glm::mat4(1), glm::mat4(1)});
    // joints.push_back({(char *)"bone_1", 0, glm::vec3(0,0,0), glm::quat(0,0,0,1), glm::vec3(1,1,1), glm::mat4(1), glm::mat4(1)});
    // joints.push_back({(char *)"bone_2", 0, glm::vec3(0,0,0), glm::quat(0,0,0,1), glm::vec3(1,1,1), glm::mat4(1), glm::mat4(1)});
    // joints.push_back({(char *)"bone_3", 0, glm::vec3(0,0,0), glm::quat(0,0,0,1), glm::vec3(1,1,1), glm::mat4(1), glm::mat4(1)});

    /**
     * - tree or array?
     * - generate joint local matrices
     * - generate joint global matrices
     */

    cgltf_skin   skin       = data->skins[0];
    cgltf_node **jointNodes = skin.joints;


    joints    = (Joint *)malloc(sizeof(Joint) * 3);
    joints[0] = {
        .name        = strdup("root"),
        .parent      = -1,
        .invBindPose = glm::mat4(1)
    };

    // joints[0].invBindPose = glm::translate(joints[0].invBindPose, 0.f, 0.f, 0.f);


    // for (size_t i = 0; i < skin.joints_count; i++) {
    //     SDL_Log("Ours: %s: 0%x", joints[i]->name, joints[i]);
    //     // for (size_t j = 0; j < joints[i]->childCount; j++) {
    //     //     SDL_Log("   child[%d]:  %s: 0%x", j, joints[i]->children[j]->name, joints[i]->children[j]);
    //     // }
    // }



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

    // cubeMesh.submeshCount = 1;
    // cubeMesh.submeshVertices.push_back(cube_verts);
    // cubeMesh.submeshIndices.push_back(cube_indices);
    // cubeMesh.Create();

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

    camera.position = { 0, 5, 16 };
    camera.forward  = { 0, 0, -1 };
    camera.up       = { 0, 1, 0 };
    camera.yaw      = -90;
    camera.pitch    = 0;

    ground.material = gMaterials["groundMaterial"];
    glGenVertexArrays(1, &ground.VAO);

    point = {
        .position = { -0.5f, -0.5f, -0.5f },
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
float fov         = 45;

void DrawPoint(glm::vec3 p, glm::vec4 color)
{
    // glBindVertexArray()
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


    glm::vec3    at         = camera.position + camera.forward;
    glm::mat4    projection = glm::perspective((fov), (float)WND_WIDTH / (float)WND_HEIGHT, .1f, 1000.f);
    glm::mat4    view       = glm::lookAt(camera.position, at, camera.up);
    static float modelYaw   = 0;
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


    ShaderUse(ground.material->_shader->programID);
    ShaderSetMat4ByName("proj", projection, ground.material->_shader->programID);
    ShaderSetMat4ByName("view", view, ground.material->_shader->programID);
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
                ShaderSetMat4ByName("projection", projection, material->_shader->programID);
                ShaderSetMat4ByName("view", view, material->_shader->programID);
                ShaderSetMat4ByName("model", model, material->_shader->programID);
                glBindTexture(GL_TEXTURE_2D, material->_texture->id);

                Mesh mesh = entities[n].model._meshes[i];
                glBindVertexArray(mesh._VAOs[j]);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh._IBOs[j]);
                glDrawElements(GL_TRIANGLES, mesh._submeshIndices[j].size(), GL_UNSIGNED_SHORT, (void *)0);
            }
        }
    }


    ShaderUse(point.material->_shader->programID);
    glm::mat4 model = glm::mat4(1);
    model           = glm::translate(model, { 1, 1, 1 });
    ShaderSetMat4ByName("projection", projection, point.material->_shader->programID);
    ShaderSetMat4ByName("view", view, point.material->_shader->programID);
    ShaderSetMat4ByName("model", model, point.material->_shader->programID);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glPointSize(6.f);
    glBindVertexArray(point.VAO);
    glDrawArrays(GL_POINTS, 0, 1);
}