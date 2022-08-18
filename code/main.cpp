#include <stdio.h>
#include <stdlib.h>
#include <string>


#define STB_IMAGE_IMPLEMENTATION
#include <adGL.h>
#include <SDL2/SDL.h>

#define VSYNC 1

struct GLVersion
{
    int Major;
    int Minor;
} gl_version;

u32 WND_WIDTH  = 1200;
u32 WND_HEIGHT = 800;

bool g_running = true;

Input input = {};

const uint64_t MAX_DT_SAMPLES = 1;

double dt_samples[MAX_DT_SAMPLES] = {};
double dt_averaged                = 0;


extern int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_EVERYTHING)) {
        SDL_Log("Video initialization failed: %s\n", SDL_GetError());
    }


    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 4);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    // SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    // SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    int           window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    SDL_Window *  window       = SDL_CreateWindow("awesome", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WND_WIDTH, WND_HEIGHT, window_flags);
    SDL_GLContext glcontext    = SDL_GL_CreateContext(window);

    if (SDL_GL_SetSwapInterval(VSYNC) < 0) {
        SDL_Log("not supported!\n");
    }

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return -1;
    }

#if defined(_DEBUG)
    SDL_Log("--Debug mode--");
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);
#endif

    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &gl_version.Major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &gl_version.Minor);
    SDL_Log("Initialized OpenGL context %d.%d Core Profile", gl_version.Major, gl_version.Minor);

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    // ExampleSPriteInit();
    Example3DInit();
    // InitSpaceShooter();


    while (g_running) {
        uint64_t start = SDL_GetPerformanceCounter();

        float startTicks = SDL_GetTicks();

        SDL_Event event = {};
        while (SDL_PollEvent(&event)) {
            SDL_Keycode key = event.key.keysym.sym;
            // SDL_Log("%d\n", key);
            switch (event.type) {

                // https://wiki.libsdl.org/SDL_Event
                //////////////////////////////
                // Key Events
                case SDL_KEYUP:
                    if (key == SDLK_ESCAPE) g_running = false;
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
                    break;
                    // END Key Events

                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == 1) input.mouse.left = true;
                    if (event.button.button == 3) input.mouse.right = true;
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == 1) input.mouse.left = false;
                    if (event.button.button == 3) input.mouse.right = false;
                    break;

                case SDL_MOUSEMOTION:
                    input.mouse.xrel = event.motion.xrel;
                    input.mouse.yrel = event.motion.yrel;
                    break;
                ///////////////////////////////

                // WIndow Events
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_CLOSE:
                            g_running = false;
                            break;
                        case SDL_WINDOWEVENT_RESIZED:
                            int w, h;
                            SDL_GL_GetDrawableSize(window, &w, &h);
                            glViewport(0, 0, w, h);
                            break;

                        default:
                            break;
                    }
                // END Window Events
                default:

                    break;
            }
        }

        static double acc = 0;
        if ((acc += dt_averaged) > 1) {
            SDL_SetWindowTitle(window, std::to_string(1 / dt_averaged).c_str());
            acc = 0;
        }

        glClearColor(.2f, .2f, .2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ExampleSpriteUpdateDraw(dt_averaged, input);
        Example3DUpdateDraw(dt_averaged, input);
        // UpdateDrawSpaceShooter(dt_averaged, input);

        SDL_GL_SwapWindow(window);

        uint64_t end = SDL_GetPerformanceCounter();

        static uint64_t idx                = 0;
        dt_samples[idx++ % MAX_DT_SAMPLES] = (end - start) / (double)SDL_GetPerformanceFrequency();

        double sum = 0;
        for (uint64_t i = 0; i < MAX_DT_SAMPLES; i++) {
            sum += dt_samples[i];
        }

        dt_averaged = sum / MAX_DT_SAMPLES;
    }

    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);

    return 0;
}
