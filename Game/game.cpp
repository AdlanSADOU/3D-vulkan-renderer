#include "pch.h"
#include "VKRenderer.h"



BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

#define dllExport __declspec(dllexport)




struct Entity
{
    FMK::Mesh          mesh;
    FMK::Transform     transform;

    // for per object things like model matrix & joint matrices.
    // this is an index that must be unique to a rendered entity as a whole
    // it is used to index ObjectData[] in a vertex shader. 
    // The index is provided as part of push constants.
    // it is also to index mapped_object_data_ptrs application side
    // it is mandatory!! So should probably be handled automatically by the rendering backend somehow!!

    // solution to the above: Entity e = e.Create()
    // when creating an entity, draw an integer from a pool
    // when destroying an entitty, restitute that integer to the pool
    int32_t object_idx;
};
std::vector<Entity> entities{};

Entity skybox;
FMK::Texture skybox_texture;
FMK::Mesh skybox_mesh;

FMK::Mesh warrior;
FMK::Mesh terrain;
FMK::Mesh hache;

FMK::Camera camera{};

extern "C" dllExport void GameInit()
{
    float aspect= FMK::WIDTH / (float)FMK::HEIGHT;
    //float aspect = 16 / 9;

    camera.CameraCreate({ 0, 0, 0 }, glm::radians(65.f), 1, .8f, 4000.f);
    camera.mode = FMK::Camera::CameraMode::THIRD_PERSON;
    camera._pitch = -40;
    SetActiveCamera(&camera);

    const int ENTITY_COUNT = 128;
    skybox_texture.CreateCubemapKTX("assets/skybox/skybox.ktx", VK_FORMAT_R8G8B8A8_UNORM);
    skybox_mesh.Create("assets/skybox/cube.gltf");
    skybox.mesh = skybox_mesh;
    skybox.object_idx = ENTITY_COUNT;
    skybox.transform.translation = { 0, -.22, 0 };
    skybox.transform.scale = glm::vec3(1.);
    skybox.mesh.push_constants.is_skybox = 1;
    //skybox.mesh._meshes[0].material_data[0].base_color_texture_idx = skybox_texture.descriptor_array_idx;

    warrior.Create("assets/warrior2/warrior.gltf");
    terrain.Create("assets/terrain/terrain_test_tile_128m.gltf");
    hache.Create("assets/hache/hache.gltf");


    entities.resize(ENTITY_COUNT);
    for (int i = 0; i < ENTITY_COUNT; i++) {
        static float z = 0;
        static float x = 0;

        int   distance_factor = 1;
        int   max_entities_on_row = 32;
        float starting_offset = -1.f;

        if (i > 0) x++;
        if (x == max_entities_on_row) {
            x = 0;
            z++;
        }

        if (i == 0) {
            entities[i].mesh = warrior;
            entities[i].transform.translation = { -10, 0.f, -10 };
            entities[i].transform.rotation = glm::quat({ 0, 0, 0 });
            entities[i].transform.scale = glm::vec3(.01f);
            entities[i].object_idx = i;

            entities[i].mesh._current_animation = &entities[i].mesh._animations[0];
            entities[i].mesh._should_play_animation = true;
        }
        else if (i == 1) {
            entities[i].mesh = terrain;
            entities[i].transform.translation = { starting_offset + x * distance_factor, -.4f, starting_offset + z * distance_factor };
            entities[i].transform.rotation = glm::quat({ 0, 0, 0 });
            entities[i].transform.scale = glm::vec3(1.0f);
            entities[i].object_idx = i;
            entities[i].mesh._meshes[0].material_data[0].tiling_x = 64;
            entities[i].mesh._meshes[0].material_data[0].tiling_y = 64;
        }
        else {
            entities[i].mesh = hache;
            entities[i].transform.translation = { starting_offset + (x - 2) * distance_factor, 1.f, starting_offset + z * distance_factor };
            entities[i].transform.rotation = glm::quat({ glm::radians(90.f), glm::radians(90.f), 0 });
            entities[i].transform.scale = glm::vec3(.1f);
            entities[i].object_idx = i;

            //entities[i].mesh = warrior;
            //// then the following line will crash
            //entities[i].transform.translation = { starting_offset + x * distance_factor, 0.f, starting_offset + z * distance_factor };
            //entities[i].transform.rotation = glm::quat({ 0, 0, 0 });
            //entities[i].transform.scale = glm::vec3(.10f);
            //entities[i].object_idx = i;

            //entities[i].mesh._current_animation = &entities[i].mesh._animations[0];
            //entities[i].mesh._should_play_animation = true;
        }
    }
}


float aspect = 1;
extern "C" dllExport void GameUpdateAndRender(float dt, FMK::Input * input)
{
    int width = 0, height = 0;
    FMK::GetWindowSize(&width, &height);
    camera.CameraUpdate(input, dt, entities[0].transform.translation);
    camera._forward.y = 0;
    if (input->Q) {
        aspect = (float)width / (float)height;
    }
    if (input->E) {
        aspect = 16 / 9;
    } 

    camera._aspect = aspect;
    SetActiveCamera(&camera);

    glm::vec3 direction{ 0, 0, 0 };

    if (input->up) {
        direction += camera._forward;
    }
    if (input->down) {
        direction -= camera._forward;
    }
    if (input->left) {
        direction -= camera._right;
    }
    if (input->right) {
        direction += camera._right;
    }


    if (glm::length(direction) > 0) {
        if (glm::length(direction) > 1.f)
            direction = glm::normalize(direction);

        if (entities[0].mesh._current_animation->handle != entities[0].mesh._animations[1].handle)
            entities[0].mesh._current_animation = &entities[0].mesh._animations[1];
        entities[0].transform.rotation = glm::quatLookAt(direction, camera._up);
        // entities[0].transform.rotation = glm::slerp(entities[0].transform.rotation, glm::quatLookAt(last_direction, camera._up), dt * 6.2f, 1);
        entities[0].transform.translation += direction * dt * 10.f * -1.f;
        static glm::vec3 last_direction = {};
        last_direction = direction;
    }
    else if (entities[0].mesh._current_animation->handle != entities[0].mesh._animations[0].handle)
        entities[0].mesh._current_animation = &entities[0].mesh._animations[0];


    FMK::BeginRendering();

    FMK::EnableDepthWrite(false);
    FMK::EnableDepthTest(false);
    FMK::SetCullMode(VK_CULL_MODE_FRONT_BIT);

    glm::mat4 skybox_model_matrix = glm::mat4(1);
    skybox_model_matrix = glm::translate(skybox_model_matrix, skybox.transform.translation) * glm::scale(skybox_model_matrix, glm::vec3(skybox.transform.scale));

    skybox.mesh.object_data.model_matrix = skybox_model_matrix;
    skybox.mesh.push_constants.object_idx = skybox.object_idx;
    FMK::SetPushConstants(&skybox.mesh.push_constants);
    FMK::SetObjectData(&skybox.mesh.object_data, skybox.object_idx);
    FMK::Draw(&skybox.mesh);

    FMK::EnableDepthWrite(true);
    FMK::EnableDepthTest(true);
    FMK::SetCullMode(VK_CULL_MODE_BACK_BIT);

    for (size_t i = 0; i < entities.size(); i++) {
        glm::mat4 model_matrix = glm::mat4(1);

        model_matrix = glm::translate(model_matrix, entities[i].transform.translation)
            * glm::mat4_cast(entities[i].transform.rotation)
            * glm::scale(model_matrix, glm::vec3(entities[i].transform.scale));

        entities[i].mesh.object_data.model_matrix = model_matrix;

        //FMK::PushConstants push_constants;
        entities[i].mesh.push_constants.object_idx = entities[i].object_idx;

        // handle animation, if any
        auto current_animation = entities[i].mesh._current_animation;
        if (current_animation) {
            AnimationUpdate(current_animation, dt);

            entities[i].mesh.push_constants.has_joints = 1;

            memcpy(
                entities[i].mesh.object_data.joint_matrices,
                current_animation->joint_matrices,
                sizeof(glm::mat4) * 128);
        }

        FMK::SetPushConstants(&entities[i].mesh.push_constants);
        FMK::SetObjectData(&entities[i].mesh.object_data, entities[i].object_idx);
        FMK::Draw(&entities[i].mesh);
    }
    FMK::EndRendering();
}