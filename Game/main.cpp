#include <vector>
#include <SDL2/SDL.h>
#include "vk_renderer.h"


struct Entity
{
    Mesh          mesh;
    ObjectData    object_data;
    PushConstants push_constants;
    Transform     _transform;

    int32_t object_idx;
};
std::vector<Entity> entities{};

Mesh warrior;
Mesh terrain;

Camera camera{};
Input input = {};

void UpdateAndRenderGame(float dt, Input* input);

extern int main(int argc, char** argv)
{
    // SetWindowDimensions(WIDTH, HEIGHT);
    InitRenderer();
    camera.CameraCreate({ 0, 10, 44 }, 45.f, 16 / (float)9, .8f, 4000.f);
    camera.mode = Camera::CameraMode::THIRD_PERSON;
    camera._pitch = -40;
    SetActiveCamera(&camera);

    warrior.Create("assets/warrior2/warrior.gltf");
    terrain.Create("assets/terrain/terrain.gltf");

    entities.resize(2);
    for (int i = 0; i < entities.size(); i++) {
        static float z = 0;
        static float x = 0;

        int   distanceFactor = 22;
        int   max_entities_on_row = 32;
        float startingOffset = 0.f;

        if (i > 0) x++;
        if (x == max_entities_on_row) {
            x = 0;
            z++;
        }

        if (i == 0) {
            entities[i].mesh = warrior;
            // then the following line will crash
            entities[i]._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale = glm::vec3(.10f);
            entities[i].object_idx = i;

            entities[i].mesh._current_animation = &entities[i].mesh._animations[0];
            entities[i].mesh._should_play_animation = true;
        }
        else if (i == 1) {
            entities[i].mesh = terrain;
            // then the following line will crash
            entities[i]._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale = glm::vec3(1000.0f);
            entities[i].object_idx = i;
            entities[i].mesh._meshes[0].material_data[0].tiling_x = 10;
            entities[i].mesh._meshes[0].material_data[0].tiling_y = 10;

        }
        else {
            entities[i].mesh = warrior;
            // then the following line will crash
            entities[i]._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale = glm::vec3(.10f);
            entities[i].object_idx = i;

            entities[i].mesh._current_animation = &entities[i].mesh._animations[0];
            entities[i].mesh._should_play_animation = true;
        }
    }




    bool bQuit = false;
    bool window_minimized = false;

    uint64_t lastCycleCount = __rdtsc();
    uint64_t lastCounter = SDL_GetPerformanceCounter();
    uint64_t perfFrequency = SDL_GetPerformanceFrequency();
    float    dt = 0;


    //
    // Main loop
    //
    SDL_Event e;
    while (!bQuit) {
        //
        // Input handling
        //
        {
            while (SDL_PollEvent(&e) != 0) {
                SDL_Keycode key = e.key.keysym.sym;

                switch (e.type) {
                case SDL_KEYUP:
                    if (key == SDLK_ESCAPE) bQuit = true;

                    if (key == SDLK_UP || key == SDLK_w) input.up = false;
                    if (key == SDLK_DOWN || key == SDLK_s) input.down = false;
                    if (key == SDLK_LEFT || key == SDLK_a) input.left = false;
                    if (key == SDLK_RIGHT || key == SDLK_d) input.right = false;
                    if (key == SDLK_q) input.Q = false;
                    if (key == SDLK_e) input.E = false;
                    break;

                case SDL_KEYDOWN:
                    if (key == SDLK_UP || key == SDLK_w) input.up = true;
                    if (key == SDLK_DOWN || key == SDLK_s) input.down = true;
                    if (key == SDLK_LEFT || key == SDLK_a) input.left = true;
                    if (key == SDLK_RIGHT || key == SDLK_d) input.right = true;
                    if (key == SDLK_q) input.Q = true;
                    if (key == SDLK_e) input.E = true;
                    if (key == SDLK_f)
                    {
                        SDL_SetWindowFullscreen(GetSDLWindowHandle(), SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }

                    break;
                    // END Key Events

                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == 1) input.mouse.left = true;
                    if (e.button.button == 3) input.mouse.right = true;
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == 1) input.mouse.left = false;
                    if (e.button.button == 3) input.mouse.right = false;
                    break;

                case SDL_MOUSEMOTION:
                    input.mouse.xrel = e.motion.xrel;
                    input.mouse.yrel = e.motion.yrel;
                    break;
                    ///////////////////////////////

                case SDL_QUIT:
                    bQuit = true;

                    // this case that embeds another switch is placed at the bottom of the outer switch
                    // because some reason the case that follows this one is always triggered on first few runs
                case SDL_WINDOWEVENT:
                    switch (e.window.event) {
                    case SDL_WINDOWEVENT_MINIMIZED:
                        window_minimized = true;
                        break;
                    case SDL_WINDOWEVENT_RESTORED:
                        if (window_minimized) window_minimized = false;
                        break;

                        // SDL_QUIT happens first
                        // case SDL_WINDOWEVENT_CLOSE:
                        //     bQuit = true;
                        //     break;

                    default:
                        break;
                    } // e.window.event
                } // e.type
            } // e
        }
        if (window_minimized) continue;


        BeginRendering();

        UpdateAndRenderGame(dt, &input);

        EndRendering();




        uint64_t endCounter = SDL_GetPerformanceCounter();
        uint64_t frameDelta = endCounter - lastCounter;

        dt = frameDelta / (float)perfFrequency;


        uint64_t endCycleCount = __rdtsc();
        uint64_t cycleDelta = endCycleCount - lastCycleCount;
        float    MCyclesPerFrame = (float)(cycleDelta / (float)(1000 * 1000));
        lastCounter = endCounter;
        lastCycleCount = endCycleCount;



        static float acc = 0;
        if ((acc += dt) > 1) {
            static char buffer[256];
            sprintf_s(buffer, sizeof(buffer), "%.2f fps | %.2f ms | %.2f MCycles | CPU: %.2f Mhz",
                1 / dt,
                dt * 1000,
                MCyclesPerFrame,
                MCyclesPerFrame / dt);

            SDL_SetWindowTitle(GetSDLWindowHandle(), buffer);
            acc = 0;
        }

    }



    /*vkDestroySurfaceKHR(gInstance, gSurface, NULL);
    vkDestroyInstance(gInstance, NULL);*/
    return 0;
}


glm::vec3 last_direction;

void UpdateAndRenderGame(float dt, Input* input)
{
    camera.CameraUpdate(input, dt, entities[0]._transform.translation);
    SetActiveCamera(&camera);
    camera._forward.y = 0;

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
        entities[0]._transform.rotation = glm::quatLookAt(direction, camera._up);
        // entities[0]._transform.rotation = glm::slerp(entities[0]._transform.rotation, glm::quatLookAt(last_direction, camera._up), dt * 6.2f, 1);
        entities[0]._transform.translation += direction * dt * 100.f * -1.f;
        last_direction = direction;
    }
    else if (entities[0].mesh._current_animation->handle != entities[0].mesh._animations[0].handle)
        entities[0].mesh._current_animation = &entities[0].mesh._animations[0];





    for (size_t i = 0; i < entities.size(); i++) {
        glm::mat4 mesh = glm::mat4(1);

        mesh = glm::translate(mesh, entities[i]._transform.translation)
            * glm::mat4_cast(entities[i]._transform.rotation)
            * glm::scale(mesh, glm::vec3(entities[i]._transform.scale));

        entities[i].object_data.model_matrix = mesh;

        PushConstants constants;
        constants.object_idx = entities[i].object_idx;

        if (entities[i].mesh._current_animation) {
            AnimationUpdate(entities[i].mesh._current_animation, dt);

            constants.has_joints = 1;
            auto joint_matrices = entities[i].mesh._current_animation->joint_matrices;
            for (size_t mat_idx = 0; mat_idx < joint_matrices.size(); mat_idx++) {
                entities[i].object_data.joint_matrices[mat_idx] = joint_matrices[mat_idx];
            }
        }

        SetObjectData(&entities[i].object_data, entities[i].object_idx);
        SetPushConstants(&constants);
        Draw(&entities[i].mesh);
    }
}