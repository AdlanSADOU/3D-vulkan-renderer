#pragma once
#include "common.h"

struct SDL_Window;

struct VKDevice
{
    SDL_Window* window;
    VkDevice         device;
    VkInstance       instance;
    VkSurfaceKHR     surface;
    VkPhysicalDevice physical_device;

    struct Queues
    {
        bool     seperate_present_queue;
        uint32_t graphics_queue_family_idx;
        uint32_t compute_queue_family_idx;
        uint32_t present_queue_family_idx;
        VkQueue  graphics;
        VkQueue  compute;
        VkQueue  present;
    } queues;

    // fix:
    // this really does not belong here
    // to get rid of it, we have to initialize SDL outside of VKDevice::Create
    int width = 1180;
    int height = 720;

    void Create(int width, int height);
};
