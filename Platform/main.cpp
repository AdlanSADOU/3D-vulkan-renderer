#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "VKRenderer.h"


#define GAME_INIT(name) void name(void)
typedef GAME_INIT(GameInit_t);

GAME_INIT(GameInit_stub) {
    // stub function that does nothing
    // to avoid crashing in case the DLL does not load properly
}

#define GAME_UPDATE_AND_RENDER(name) void name(float dt, FMK::Input *input)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRender_t);

GAME_UPDATE_AND_RENDER(GameUpdateAndRender_stub) {
    // stub function that does nothing
    // to avoid crashing in case the DLL does not load properly
}


struct GameDll
{
    HMODULE handle;
    GameInit_t* GameInit;
    GameUpdateAndRender_t* GameUpdateAndRender;
};

void LoadGameDll(GameDll* dll)
{
    dll->GameInit = GameInit_stub;

    if (!(dll->handle = LoadLibraryA("Game.dll")))
    {
        SDL_Log(" 'LoadLibraryA' failed with code: %d\n", GetLastError());
    }

    dll->GameInit = (GameInit_t*)GetProcAddress(dll->handle, "GameInit");
    dll->GameUpdateAndRender = (GameUpdateAndRender_t*)GetProcAddress(dll->handle, "GameUpdateAndRender");

    if (!dll->GameInit) {
        SDL_Log(" 'GetProcAddress' failed with code: %d\n", GetLastError());
        dll->GameInit = GameInit_stub;
    }

    if (!dll->GameUpdateAndRender) {
        SDL_Log(" 'GetProcAddress' failed with code: %d\n", GetLastError());
        dll->GameUpdateAndRender = GameUpdateAndRender_stub;
    }
}


int width = 1280;
int height = 640;

FMK::Input input;

extern int main(int argc, char** argv)
{
    FMK::InitRenderer(width, height);

    GameDll game_dll;
    LoadGameDll(&game_dll);
    game_dll.GameInit();


    bool bQuit = false;
    bool bWindow_minimized = false;
    bool bFullscreen = false;

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
                        if (!bFullscreen)
                        {
                            SDL_SetWindowFullscreen(FMK::GetSDLWindowHandle(), SDL_WINDOW_FULLSCREEN_DESKTOP);
                            bFullscreen = true;
                        }
                        else if (bFullscreen) {
                            SDL_SetWindowFullscreen(FMK::GetSDLWindowHandle(), 0);
                            bFullscreen = false;
                        }
                        

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

                    // this case embedding another switch is placed at the bottom of the outer switch
                    // because for some reason the case that follows this one is always triggered on first few runs
                case SDL_WINDOWEVENT:
                    switch (e.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        SDL_GetWindowSize(FMK::GetSDLWindowHandle(), &width, &height);
                        //FMK::SetWindowSize(width, height);

                        break;
                    case SDL_WINDOWEVENT_MINIMIZED:
                        bWindow_minimized = true;
                        break;
                    case SDL_WINDOWEVENT_RESTORED:
                        if (bWindow_minimized) bWindow_minimized = false;
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
        if (bWindow_minimized) continue;


        if (input.E) {
            FMK::RebuildGraphicsPipeline();
        }


        game_dll.GameUpdateAndRender(dt, &input);


        uint64_t endCounter = SDL_GetPerformanceCounter();
        uint64_t frameDelta = endCounter - lastCounter;

        dt = frameDelta / (float)perfFrequency;

        uint64_t endCycleCount = __rdtsc();
        uint64_t cycleDelta = endCycleCount - lastCycleCount;
        float    MCyclesPerFrame = (float)(cycleDelta / (float)(1000 * 1000));
        lastCounter = endCounter;
        lastCycleCount = endCycleCount;

#if 1
        static float acc = 0;
        if ((acc += dt) > 1) {
            static char buffer[256];
            sprintf_s(buffer, sizeof(buffer), "%.2f fps | %.2f ms | %.2f MCycles | CPU: %.2f Mhz",
                1 / dt,
                dt * 1000,
                MCyclesPerFrame,
                MCyclesPerFrame / dt);

            SDL_SetWindowTitle(FMK::GetSDLWindowHandle(), buffer);
            //SDL_Log("%s", buffer);
            acc = 0;
        }
#endif
    }

    /*vkDestroySurfaceKHR(gInstance, gSurface, NULL);
    vkDestroyInstance(gInstance, NULL);*/
    return 0;
}


