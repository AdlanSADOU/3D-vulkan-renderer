#include <vector>
#include <SDL2/SDL.h>
#include "vk_renderer.h"
#include <windows.h>

struct Entity
{
    Renderer::Mesh          mesh;
    Renderer::ObjectData    object_data;
    Renderer::PushConstants push_constants;
    Renderer::Transform     transform;

    // for per object things like model matrix & joint matrices
    // this is an index that must be unique to a rendered entity as a whole
    // it is used to index ObjectData[] inside a vertex shader provided as part of push constants
    // and also to index mapped_object_data_ptrs
    // it is mandatory!! So should probably be handled automatically by the rendering backend somehow!!
    int32_t object_idx;
};
std::vector<Entity> entities{};

Entity skybox;
Renderer::Texture skybox_texture;
Renderer::Mesh skybox_mesh;

Renderer::Mesh warrior;
Renderer::Mesh terrain;
Renderer::Mesh hache;

Renderer::Camera camera{};
Renderer::Input input = {}; // fixme: tied to Renderer because Camera uses it!!

void UpdateAndRenderGame(float dt, Renderer::Input* input);
int width = 1180;
int height = 720;

extern int main(int argc, char** argv)
{
    Renderer::SetWindowSize(width, height);
    Renderer::InitRenderer();

    camera.CameraCreate({ 0, 0, 0 }, glm::radians(65.f), width / (float)height, .8f, 4000.f);
    camera.mode = Renderer::Camera::CameraMode::THIRD_PERSON;
    camera._pitch = -40;
    SetActiveCamera(&camera);

    skybox_texture.CreateCubemapKTX("assets/skybox/skybox.ktx", VK_FORMAT_R8G8B8A8_UNORM);
    skybox_mesh.Create("assets/skybox/cube.gltf");
    skybox.mesh = skybox_mesh;
    skybox.object_idx = 1024 - 1;
    skybox.transform.translation = { 0, -.22, 0 };
    skybox.transform.scale = glm::vec3(1.);
    skybox.push_constants.is_skybox = 1;
    //skybox.mesh._meshes[0].material_data[0].base_color_texture_idx = skybox_texture.descriptor_array_idx;

    warrior.Create("assets/warrior2/warrior.gltf");
    terrain.Create("assets/terrain/terrain.gltf");
    hache.Create("assets/hache/hache.gltf");

    entities.resize(3);
    for (int i = 0; i < entities.size(); i++) {
        static float z = 0;
        static float x = 0;

        int   distanceFactor = 16;
        int   max_entities_on_row = 64;
        float startingOffset = -100.f;

        if (i > 0) x++;
        if (x == max_entities_on_row) {
            x = 0;
            z++;
        }

        if (i == 0) {
            entities[i].mesh = warrior;
            // then the following line will crash
            entities[i].transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i].transform.rotation = glm::quat({ 0, 0, 0 });
            entities[i].transform.scale = glm::vec3(.10f);
            entities[i].object_idx = i;

            entities[i].mesh._current_animation = &entities[i].mesh._animations[0];
            entities[i].mesh._should_play_animation = true;
        }
        else if (i == 1) {
            entities[i].mesh = terrain;
            // then the following line will crash
            entities[i].transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i].transform.rotation = glm::quat({ 0, 0, 0 });
            entities[i].transform.scale = glm::vec3(1000.0f);
            entities[i].object_idx = i;
            entities[i].mesh._meshes[0].material_data[0].tiling_x = 32;
            entities[i].mesh._meshes[0].material_data[0].tiling_y = 32;
        }
        else {
            entities[i].mesh = hache;
            // then the following line will crash
            entities[i].transform.translation = { startingOffset + (x - 2) * distanceFactor, 16.f, startingOffset + z * distanceFactor };
            entities[i].transform.rotation = glm::quat({ glm::radians(90.f), glm::radians(90.f), 0 });
            entities[i].transform.scale = glm::vec3(3);
            entities[i].object_idx = i;

            //entities[i].mesh = warrior;
            //// then the following line will crash
            //entities[i].transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            //entities[i].transform.rotation = glm::quat({ 0, 0, 0 });
            //entities[i].transform.scale = glm::vec3(.10f);
            //entities[i].object_idx = i;

            //entities[i].mesh._current_animation = &entities[i].mesh._animations[0];
            //entities[i].mesh._should_play_animation = true;
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
                        SDL_SetWindowFullscreen(Renderer::GetSDLWindowHandle(), SDL_WINDOW_FULLSCREEN_DESKTOP);
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
                    case SDL_WINDOWEVENT_RESIZED:
                        SDL_GetWindowSize(Renderer::GetSDLWindowHandle(), &width, &height);
                        break;
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



        if (input.E) {
            Renderer::CreateGraphicsPipeline(Renderer::gDevice, &Renderer::gDefault_graphics_pipeline);
        }


        Renderer::BeginRendering();
        UpdateAndRenderGame(dt, &input);
        Renderer::EndRendering();

        //SDL_Log("camp: (%f, %f, %f)\n", camera._position.x, camera._position.y, camera._position.z);



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

            SDL_SetWindowTitle(Renderer::GetSDLWindowHandle(), buffer);
            acc = 0;
        }

    }

    /*vkDestroySurfaceKHR(gInstance, gSurface, NULL);
    vkDestroyInstance(gInstance, NULL);*/
    return 0;
}


glm::vec3 last_direction;

void UpdateAndRenderGame(float dt, Renderer::Input* input)
{
    camera.CameraUpdate(input, dt, entities[0].transform.translation);
    camera._forward.y = 0;
    camera._aspect = width / (float)height;
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
        entities[0].transform.translation += direction * dt * 74.f * -1.f;
        last_direction = direction;
    }
    else if (entities[0].mesh._current_animation->handle != entities[0].mesh._animations[0].handle)
        entities[0].mesh._current_animation = &entities[0].mesh._animations[0];


    Renderer::EnableDepthWrite(false);
    Renderer::EnableDepthTest(false);
    Renderer::SetCullMode(VK_CULL_MODE_FRONT_BIT);
    glm::mat4 skybox_model_matrix = glm::mat4(1);
    skybox_model_matrix = glm::translate(skybox_model_matrix, skybox.transform.translation) * glm::scale(skybox_model_matrix, glm::vec3(skybox.transform.scale));

    skybox.object_data.model_matrix = skybox_model_matrix;
    skybox.push_constants.object_idx = skybox.object_idx;
    Renderer::SetPushConstants(&skybox.push_constants);
    Renderer::SetObjectData(&skybox.object_data, skybox.object_idx);
    Renderer::Draw(&skybox.mesh);

    Renderer::EnableDepthWrite(true);
    Renderer::EnableDepthTest(true);
    Renderer::SetCullMode(VK_CULL_MODE_BACK_BIT);

    for (size_t i = 0; i < entities.size(); i++) {
        glm::mat4 model_matrix = glm::mat4(1);

        model_matrix = glm::translate(model_matrix, entities[i].transform.translation)
            * glm::mat4_cast(entities[i].transform.rotation)
            * glm::scale(model_matrix, glm::vec3(entities[i].transform.scale));

        entities[i].object_data.model_matrix = model_matrix;

        Renderer::PushConstants push_constants;
        push_constants.object_idx = entities[i].object_idx;

        // handle animation, if any
        auto current_animation = entities[i].mesh._current_animation;
        if (current_animation) {
            AnimationUpdate(current_animation, dt);

            push_constants.has_joints = 1;

            memcpy(
                entities[i].object_data.joint_matrices,
                current_animation->joint_matrices,
                sizeof(glm::mat4) * Renderer::MAX_JOINTS);
        }

        Renderer::SetPushConstants(&push_constants);
        Renderer::SetObjectData(&entities[i].object_data, entities[i].object_idx);
        Renderer::Draw(&entities[i].mesh);
    }
}