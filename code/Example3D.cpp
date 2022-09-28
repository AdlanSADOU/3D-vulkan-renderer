#include "backend.h"

#if defined(GL) && !defined(VULKAN)

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
    camera.CameraCreate({ 16, 20, 44 }, 45.f, WND_WIDTH / (float)WND_HEIGHT, .8f, 4000.f);
    camera._pitch = -20;
    gCameraInUse  = &camera;


    gTextures.insert({ "ground", TextureCreate("assets/groundProto.png") });

    // todo: cache shaders in MaterialCreate
    gMaterials.insert({ "default", MaterialCreate("shaders/standard.shader", gTextures["ground"]) });
    gMaterials.insert({ "groundMaterial", MaterialCreate("shaders/ground.shader", gTextures["ground"]) });



    entities.resize(256);
    for (size_t i = 0; i < entities.size(); i++) {
        static float z = 0;
        static float x = 0;

        int   distanceFactor      = 24;
        int   max_entities_on_row = 12;
        float startingOffset      = 0.f;

        if (i > 0) x++;
        if (x == max_entities_on_row) {
            x = 0;
            z++;
        }

        if (1) {
            entities[i].model.Create("assets/warrior/warrior.gltf"); // fixme: if for some reason this fails to load
            // then the following line will crash
            entities[i].model._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i].model._transform.rotation    = glm::quat({0,0,0});
            entities[i].model._transform.scale       = glm::vec3(10.1f);
            entities[i].model._current_animation      = &entities[i].model._animations[0];
            entities[i].model._should_play_animation   = false;
        }
         else if (i == 1) {
            entities[i].model.Create("assets/capoera.gltf"); // fixme: if for some reason this fails to load
            // then the following line will crash
            entities[i].model._meshes[0]._materials[0] = gMaterials["default"];
            entities[i].model._transform.translation   = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i].model._transform.rotation      = glm::quat({0,0,0});
            entities[i].model._transform.scale         = glm::vec3(0.1f);
            entities[i].model._current_animation        = &entities[i].model._animations[i % 2];
            entities[i].model._should_play_animation     = true;
        } else if (i == 2) {
            entities[i].model.Create("assets/chibi_02_ex.gltf"); // fixme: if for some reason this fails to load
            // then the following line will crash
            entities[i].model._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i].model._transform.rotation    = glm::quat({0,0,0});
            entities[i].model._transform.scale       = glm::vec3(10.1f);
            entities[i].model._current_animation      = &entities[i].model._animations[0];
            entities[i].model._should_play_animation   = true;
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

    glBindTexture(GL_TEXTURE_2D, ground.material->base_color_map->id);
    glBindVertexArray(ground.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    //
    // Entities
    //


    // glMultiDrawElementsIndirect()

    for (size_t i = 0; i < entities.size(); i++) {
        entities[i].model.AnimationUpdate(dt);
        entities[i].model.Draw(dt);
    }
}
#endif