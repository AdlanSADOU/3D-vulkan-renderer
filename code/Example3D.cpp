#define CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "adgl.h"

#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "Animation.h"
#include "Mesh.h"
#include "Point.h"


// todo: should these be provided as globals?
// how should we deal with general window parameters?
extern uint32_t WND_WIDTH;
extern uint32_t WND_HEIGHT;

static Camera camera = {};

/** note:
 * we might want to find a proper way to handle
 * assets down the road if this becomes a problem
 */

struct Ground
{
    Material *material;
    uint32_t  VAO;
} ground;

struct Entity
{
    SkinnedModel model;
};

std::vector<Entity> entities {};

void Example3DInit()
{
    camera.CameraCreate({ 0, 40, 50 }, 45.f, WND_WIDTH / (float)WND_HEIGHT, .8f, 4000.f);
    camera._pitch = -25;
    gCameraInUse  = &camera;


    gTextures.insert({ "warrior", TextureCreate("assets/warrior/MaleBruteA_Body_diffuse.png") });
    gTextures.insert({ "CesiumMan", TextureCreate("assets/CesiumMan_img0.jpg") });
    gTextures.insert({ "brick_wall", TextureCreate("assets/brick_wall.jpg") });
    gTextures.insert({ "cs_blend_red Base Color", TextureCreate("assets/cs_blend_red Base Color.png") });
    gTextures.insert({ "ground", TextureCreate("assets/groundProto.png") });
    gTextures.insert({ "cube", TextureCreate("assets/cube.png") });
    gTextures.insert({ "Material-1", TextureCreate("assets/Material-1.png") });
    gTextures.insert({ "Material-2", TextureCreate("assets/Material-2.png") });
    gTextures.insert({ "Material", TextureCreate("assets/Material.png") });


    // todo: cache shaders in MaterialCreate
    gMaterials.insert({ "standard", MaterialCreate("shaders/point.shader", NULL) });
    gMaterials.insert({ "warrior", MaterialCreate("shaders/standard.shader", gTextures["ground"]) });
    gMaterials.insert({ "groundMaterial", MaterialCreate("shaders/ground.shader", gTextures["ground"]) });



    entities.resize(14);
    for (size_t i = 0; i < entities.size(); i++) {
        static float z = 0;
        static float x = 0;

        int   distanceFactor      = 24;
        int   max_entities_on_row = 9;
        float startingOffset      = 0.f;

        if (i > 0) x++;
        if (x == max_entities_on_row) {
            x = 0;
            z++;
        }

        if (1)
        {
            entities[i].model.Create("assets/warrior/warrior.gltf"); // todo: if for some reason this fails to load
            // then the following line will crash
            entities[i].model._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i].model._transform.rotation    = { 0.f, 0.f, 0.f };
            entities[i].model._transform.scale       = 0.1f;
            entities[i].model.currentAnimation       = &entities[i].model._animations[0];
        } else {
            entities[i].model.Create("assets/capoera.gltf"); // todo: if for some reason this fails to load
            // then the following line will crash
            entities[i].model._meshes[0]._materials[0] = gMaterials["warrior"];
            entities[i].model._transform.translation   = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i].model._transform.rotation      = { 0.f, 0.f, 0.f };
            entities[i].model._transform.scale         = .1f;
            entities[i].model.currentAnimation         = &entities[i].model._animations[i % 2];
        }
    }

    ground.material = gMaterials["groundMaterial"];
    glGenVertexArrays(1, &ground.VAO);
}


void Example3DUpdateDraw(float dt, Input input)
{
    gCameraInUse->_aspect = WND_WIDTH / (float)WND_HEIGHT;
    gCameraInUse->CameraUpdate(input, dt);

    // draw wireframe
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //
    // Ground plane
    //
    ShaderUse(ground.material->_shader->programID);
    ShaderSetMat4ByName("proj", gCameraInUse->_projection, ground.material->_shader->programID);
    ShaderSetMat4ByName("view", gCameraInUse->_view, ground.material->_shader->programID);

    glBindTexture(GL_TEXTURE_2D, ground.material->baseColorMap->id);
    glBindVertexArray(ground.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    //
    // Entities
    //
    for (size_t i = 0; i < entities.size(); i++) {
        entities[i].model.AnimationUpdate(dt);
        entities[i].model.Draw();
    }
}