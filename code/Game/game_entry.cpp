#include <vector>

#include "vk_engine.h"

Camera camera {};
struct Entity
{
    Mesh          model;
    ObjectData    object_data;
    PushConstants push_constants;
    Transform     _transform;

    int32_t object_idx;
};
std::vector<Entity> entities {};

Mesh warrior;
Mesh terrain;


void InitGame()
{
    // SetWindowDimensions(WIDTH, HEIGHT);

    camera.CameraCreate({ 0, 10, 44 }, 45.f, 16 / (float)9, .8f, 4000.f);
    camera.mode   = Camera::CameraMode::THIRD_PERSON;
    camera._pitch = -40;
    SetActiveCamera(&camera);

    warrior.Create("assets/warrior2/warrior.gltf");
    terrain.Create("assets/terrain/terrain.gltf");

    entities.resize(2);
    for (size_t i = 0; i < entities.size(); i++) {
        static float z = 0;
        static float x = 0;

        int   distanceFactor      = 22;
        int   max_entities_on_row = 32;
        float startingOffset      = 0.f;

        if (i > 0) x++;
        if (x == max_entities_on_row) {
            x = 0;
            z++;
        }

        if (i == 0) {
            entities[i].model = warrior;
            // then the following line will crash
            entities[i]._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation    = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale       = glm::vec3(.10f);
            entities[i].object_idx             = i;

            entities[i].model._current_animation     = &entities[i].model._animations[0];
            entities[i].model._should_play_animation = true;
        } else if (i == 1) {
            entities[i].model = terrain;
            // then the following line will crash
            entities[i]._transform.translation                     = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation                        = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale                           = glm::vec3(1000.0f);
            entities[i].object_idx                                 = i;
            entities[i].model._meshes[0].material_data[0].tiling_x = 10;
            entities[i].model._meshes[0].material_data[0].tiling_y = 10;

        } else {
            entities[i].model = warrior;
            // then the following line will crash
            entities[i]._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation    = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale       = glm::vec3(.10f);
            entities[i].object_idx             = i;

            entities[i].model._current_animation     = &entities[i].model._animations[0];
            entities[i].model._should_play_animation = true;
        }
    }
}


glm::vec3 last_direction;
void      UpdateAndRenderGame(float dt, Input *input)
{
    camera.CameraUpdate(input, dt, entities[0]._transform.translation);
    SetActiveCamera(&camera);
    camera._forward.y = 0;

    glm::vec3 direction { 0, 0, 0 };

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

        if (entities[0].model._current_animation->handle != entities[0].model._animations[1].handle)
            entities[0].model._current_animation = &entities[0].model._animations[1];
        entities[0]._transform.rotation = glm::quatLookAt(direction, camera._up);
        // entities[0]._transform.rotation = glm::slerp(entities[0]._transform.rotation, glm::quatLookAt(last_direction, camera._up), dt * 6.2f, 1);
        entities[0]._transform.translation += direction * dt * 100.f * -1.f;
        last_direction = direction;
    } else if (entities[0].model._current_animation->handle != entities[0].model._animations[0].handle)
        entities[0].model._current_animation = &entities[0].model._animations[0];





    for (size_t i = 0; i < entities.size(); i++) {
        glm::mat4 model = glm::mat4(1);

        model = glm::translate(model, entities[i]._transform.translation)
            * glm::mat4_cast(entities[i]._transform.rotation)
            * glm::scale(model, glm::vec3(entities[i]._transform.scale));


        entities[i].object_data.model = model;

        PushConstants constants;
        constants.object_idx = entities[i].object_idx;

        if (entities[i].model._current_animation) {
            AnimationUpdate(&entities[i].model, dt);

            constants.has_joints = 1;
            auto joint_matrices  = entities[i].model._current_animation->joint_matrices;
            for (size_t mat_idx = 0; mat_idx < joint_matrices.size(); mat_idx++) {
                entities[i].object_data.joint_matrices[mat_idx] = joint_matrices[mat_idx];
            }
        }

        SetObjectData(&entities[i].object_data, entities[i].object_idx);
        SetPushConstants(&constants);
        Draw(&entities[i].model);
    }
}