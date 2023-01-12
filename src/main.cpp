#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <SDL2/SDL.h>
#include "vk_renderer.h"


int width = 1180;
int height = 720;

#define GAME_INIT(name) void name(void)
typedef GAME_INIT(GameInit_t);
GAME_INIT(game_init_stub) {}

#define GAME_UPDATE_AND_RENDER(name) void name(float dt, FMK::Input *input)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRender_t);
GAME_UPDATE_AND_RENDER(game_update_and_render_stub) {}

FMK::Input input;

struct GameDll
{
    HMODULE handle;
    GameInit_t* game_init;
    GameUpdateAndRender_t* game_update_and_render;
};

void LoadGameDll(GameDll* dll)
{
    dll->game_init = game_init_stub;

    dll->handle = LoadLibraryA("Game.dll");
    if (dll->handle)
    {
        dll->game_init = (GameInit_t*)GetProcAddress(dll->handle, "GameInit");
        dll->game_update_and_render = (GameUpdateAndRender_t*)GetProcAddress(dll->handle , "GameUpdateAndRender");

        if (!dll->game_init) {
            SDL_Log(" 'GetProcAddress': %d\n", GetLastError());
            dll->game_init = game_init_stub;
        }

        if (!dll->game_update_and_render) {
            SDL_Log(" 'GetProcAddress': %d\n", GetLastError());
            dll->game_update_and_render = game_update_and_render_stub;
        }
    }
}

extern int main(int argc, char** argv)
{
    GameDll game_dll;
    LoadGameDll(&game_dll);
    
    FMK::SetWindowSize(width, height);
    FMK::InitVulkanRenderer();


    game_dll.game_init();

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
                        SDL_SetWindowFullscreen(FMK::GetSDLWindowHandle(), SDL_WINDOW_FULLSCREEN_DESKTOP);
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
                    // because for some reason the case that follows this one is always triggered on first few runs
                case SDL_WINDOWEVENT:
                    switch (e.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        SDL_GetWindowSize(FMK::GetSDLWindowHandle(), &width, &height);
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
            FMK::CreateGraphicsPipeline(FMK::gDevice, &FMK::gDefault_graphics_pipeline);
        }



        game_dll.game_update_and_render(dt, &input);


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

            //SDL_SetWindowTitle(Renderer::GetSDLWindowHandle(), buffer);
            SDL_Log("%s", buffer);
            acc = 0;
        }

    }

    /*vkDestroySurfaceKHR(gInstance, gSurface, NULL);
    vkDestroyInstance(gInstance, NULL);*/
    return 0;
}


