#include "VKRenderer.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>



BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

#define dllExport __declspec(dllexport)



// see comment below
int32_t gObject_index = 0;

struct Entity
{
    FMK::Mesh          mesh;
    FMK::Transform     transform;

    // Used to access per object data like model matrix & joint matrices in shaders.
    // this is an index that must be unique to a rendered entity as a whole
    // it is used to index ObjectData[] in a vertex shader. 
    // The index is provided as part of push constants.
    // it is also to index mapped_object_data_ptrs application side
    // it is mandatory!! So should probably be handled automatically by the rendering backend somehow!!

    // solution to the above: Entity e = e.Create()
    // when creating an entity, draw an integer from a pool
    // when destroying an entitty, restitute that integer to the pool
    int32_t object_idx;

    Entity() {};
};
std::vector<Entity> entities{};

Entity skybox;
Entity *player;

FMK::Texture skybox_texture;
FMK::Mesh skybox_mesh;

FMK::Mesh mesh_warrior;
FMK::Mesh mesh_paladin;
FMK::Mesh mesh_terrain;
FMK::Mesh mesh_hache;

FMK::Camera camera{};

extern "C" dllExport void GameInit()
{
    float aspect = FMK::WIDTH / (float)FMK::HEIGHT;

    camera.CameraCreate({ 0, 2, 0 }, glm::radians(60.f), aspect, .4f, 1000.f);
    camera.mode = FMK::Camera::CameraMode::THIRD_PERSON;
    camera._pitch = -40;
    SetActiveCamera(&camera);


    const int ENTITY_COUNT = 128;
    skybox_texture.CreateCubemapKTX("assets/skybox/skybox.ktx", VK_FORMAT_R8G8B8A8_UNORM);
    skybox_mesh.Create("assets/skybox/cube.gltf");
    skybox.mesh = skybox_mesh;
    skybox.object_idx = ENTITY_COUNT;
    skybox.transform.mTranslation = { 0, -.22, 0 };
    skybox.transform.mScale = glm::vec3(1.);
    skybox.mesh.push_constants.is_skybox = 1;
    //skybox.mesh._meshes[0].material_data[0].base_color_texture_idx = skybox_texture.descriptor_array_idx;


    mesh_warrior.Create("assets/warrior2/warrior.gltf");
    mesh_paladin.Create("assets/paladin/paladin.gltf");
    mesh_terrain.Create("assets/terrain/terrain_test_tile_128m_2.gltf");
    mesh_hache.Create("assets/hache/hache.gltf");


    //note: temporary lambda, I have not decided yet how I want entities to work.
    auto createEntity = [](FMK::Mesh mesh, FMK::Transform transform, int32_t *objectIndex) -> Entity {
        Entity e{};

        e.mesh = mesh;
        e.transform = transform;

        if (mesh._animations.size() > 0)
        {
            e.mesh._should_play_animation = true;
            e.mesh._current_animation_idx = 0;
        }
        else {
            e.mesh._should_play_animation = false;
            e.mesh._current_animation_idx = -1;
        }

        assert(*objectIndex >= 0 && "negative objectIndex");
        e.object_idx = *(objectIndex);
        *(objectIndex) += 1;

        return e;
    };


    // Warrior entity
    {
        auto transform = FMK::Transform(
            glm::vec3(0, 0, -20.f),
            glm::quat({ 0, 0, 0 }),
            glm::vec3(0.01f));

        entities.push_back(createEntity(mesh_warrior, transform, &gObject_index));
    }

    // Terrain
    {
        Entity e;

        auto transform = FMK::Transform(
            glm::vec3(0, 0, 0),
            glm::quat({ 0, 0, 0 }),
            glm::vec3(1.f));

        e = createEntity(mesh_terrain, transform, &gObject_index);

        e.mesh._meshes[0].material_data[0].tiling_x = 32;
        e.mesh._meshes[0].material_data[0].tiling_y = 32;

        entities.push_back(e);
    }

    // Paladins
    for (int i = 0; i < ENTITY_COUNT - 64; i++) {
        static float z = 0;
        static float x = 0;

        int   distance_factor = 3;
        int   max_entities_on_row = 16;
        float starting_offset = -14.f;

        if (i > 0) x++;
        if (x == max_entities_on_row) {
            x = 0;
            z++;
        }

        auto transform = FMK::Transform(
            glm::vec3(starting_offset + (x - 2) * distance_factor, 0, z * distance_factor),
            glm::quat({ glm::radians(0.f), glm::radians(0.f), 0 }),
            glm::vec3(0.014f));

        Entity e;

        e = createEntity(mesh_paladin, transform, &gObject_index);
        entities.push_back(e);
    }
}



extern "C" dllExport void GameUpdateAndRender(float dt, FMK::Input * input)
{
    int width = 0, height = 0;
    FMK::GetWindowSize(&width, &height);
    camera.CameraUpdate(input, dt, entities[0].transform.mTranslation);
    camera._forward.y = 0;
    camera._aspect = width / (float)height;
    SetActiveCamera(&camera);


    glm::vec3 inputDirection{ 0, 0, 0 };

    if (input->up) {
        inputDirection += camera._forward;
    }
    if (input->down) {
        inputDirection -= camera._forward;
    }
    if (input->left) {
        inputDirection -= camera._right;
    }
    if (input->right) {
        inputDirection += camera._right;
    }


    if (glm::length(inputDirection) > 0) {
        if (glm::length(inputDirection) > 1.f)
            inputDirection = glm::normalize(inputDirection);

        if (entities[0].mesh._current_animation_idx != 1)
            entities[0].mesh._current_animation_idx = 1;

        entities[0].transform.mRotation = glm::quatLookAt(inputDirection, camera._up);
        //entities[0].transform.rotation = glm::slerp(entities[0].transform.rotation, glm::quatLookAt(last_direction, camera._up), dt * 6.2f, 1);
        entities[0].transform.mTranslation += inputDirection * dt * 10.f * -1.f;
    }
    else if (entities[0].mesh._current_animation_idx != 0)
        entities[0].mesh._current_animation_idx = 0;


    FMK::BeginRendering();


    // Draw skybox
    FMK::EnableDepthWrite(false);
    //FMK::EnableDepthTest(false);
    FMK::SetCullMode(VK_CULL_MODE_FRONT_BIT);

    glm::mat4 skybox_model_matrix = glm::mat4(1);
    skybox_model_matrix = glm::translate(skybox_model_matrix, skybox.transform.mTranslation) * glm::scale(skybox_model_matrix, glm::vec3(skybox.transform.mScale));

    skybox.mesh.object_data.model_matrix = skybox_model_matrix;
    skybox.mesh.push_constants.object_idx = skybox.object_idx;
    FMK::SetPushConstants(&skybox.mesh.push_constants);
    FMK::SetObjectData(&skybox.mesh.object_data, skybox.object_idx);
    FMK::Draw(&skybox.mesh);

    FMK::EnableDepthWrite(true);
    FMK::EnableDepthTest(true);
    FMK::SetCullMode(VK_CULL_MODE_BACK_BIT);

    // Render entities
    for (size_t i = 0; i < entities.size(); i++) {
        glm::mat4 model_matrix = glm::mat4(1);

        model_matrix = glm::translate(model_matrix, entities[i].transform.mTranslation)
            * glm::mat4_cast(entities[i].transform.mRotation)
            * glm::scale(model_matrix, glm::vec3(entities[i].transform.mScale));

        // ObjectData is a struct that is passed down to the vertex shader.
        // it holds per object(an entire mesh) draw data.
        // Like the model matrix and joint matrices.
        entities[i].mesh.object_data.model_matrix = model_matrix;

        // handle animation, if any

        if (entities[i].mesh._should_play_animation) {
            auto animations = &entities[i].mesh._animations;
            auto index = entities[i].mesh._current_animation_idx;
            AnimationUpdate(&(*animations)[index], dt);

            entities[i].mesh.push_constants.has_joints = 1;

            memcpy(
                entities[i].mesh.object_data.joint_matrices,
                (*animations)[index].joint_matrices,
                sizeof(glm::mat4) * 128);
        }

        entities[i].mesh.push_constants.object_idx = entities[i].object_idx;

        FMK::SetPushConstants(&entities[i].mesh.push_constants);
        FMK::SetObjectData(&entities[i].mesh.object_data, entities[i].object_idx);
        FMK::Draw(&entities[i].mesh);
    }
    FMK::EndRendering();
}