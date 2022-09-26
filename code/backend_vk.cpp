#include "backend.h"

#if defined(VULKAN) && !defined(GL)
#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>


static std::unordered_map<VkResult, std::string> vulkan_errors = {
    { (VkResult)0, "VK_SUCCESS" },
    { (VkResult)1, "VK_NOT_READY" },
    { (VkResult)2, "VK_TIMEOUT" },
    { (VkResult)3, "VK_EVENT_SET" },
    { (VkResult)4, "VK_EVENT_RESET" },
    { (VkResult)5, "VK_INCOMPLETE" },
    { (VkResult)-1, "VK_ERROR_OUT_OF_HOST_MEMORY" },
    { (VkResult)-2, "VK_ERROR_OUT_OF_DEVICE_MEMORY" },
    { (VkResult)-3, "VK_ERROR_INITIALIZATION_FAILED" },
    { (VkResult)-4, "VK_ERROR_DEVICE_LOST" },
    { (VkResult)-5, "VK_ERROR_MEMORY_MAP_FAILED" },
    { (VkResult)-6, "VK_ERROR_LAYER_NOT_PRESENT" },
    { (VkResult)-7, "VK_ERROR_EXTENSION_NOT_PRESENT" },
    { (VkResult)-8, "VK_ERROR_FEATURE_NOT_PRESENT" },
    { (VkResult)-9, "VK_ERROR_INCOMPATIBLE_DRIVER" },
    { (VkResult)-10, "VK_ERROR_TOO_MANY_OBJECTS" },
    { (VkResult)-11, "VK_ERROR_FORMAT_NOT_SUPPORTED" },
    { (VkResult)-12, "VK_ERROR_FRAGMENTED_POOL" },
    { (VkResult)-13, "VK_ERROR_UNKNOWN" },
    { (VkResult)-1000069000, "VK_ERROR_OUT_OF_POOL_MEMORY" },
    { (VkResult)-1000072003, "VK_ERROR_INVALID_EXTERNAL_HANDLE" },
    { (VkResult)-1000161000, "VK_ERROR_FRAGMENTATION" },
    { (VkResult)-1000257000, "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS" },
    { (VkResult)-1000000000, "VK_ERROR_SURFACE_LOST_KHR" },
    { (VkResult)-1000000001, "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR" },
    { (VkResult)1000001003, "VK_SUBOPTIMAL_KHR" },
    { (VkResult)-1000001004, "VK_ERROR_OUT_OF_DATE_KHR" },
    { (VkResult)-1000003001, "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR" },
    { (VkResult)-1000011001, "VK_ERROR_VALIDATION_FAILED_EXT" },
    { (VkResult)-1000012000, "VK_ERROR_INVALID_SHADER_NV" },
    { (VkResult)-1000158000, "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT" },
    { (VkResult)-1000174001, "VK_ERROR_NOT_PERMITTED_EXT" },
    { (VkResult)-1000255000, "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT" },
    { (VkResult)1000268000, "VK_THREAD_IDLE_KHR" },
    { (VkResult)1000268001, "VK_THREAD_DONE_KHR" },
    { (VkResult)1000268002, "VK_OPERATION_DEFERRED_KHR" },
    { (VkResult)1000268003, "VK_OPERATION_NOT_DEFERRED_KHR" },
    { (VkResult)1000297000, "VK_PIPELINE_COMPILE_REQUIRED_EXT" },
    { VK_ERROR_OUT_OF_POOL_MEMORY, "VK_ERROR_OUT_OF_POOL_MEMORY_KHR" },
    { VK_ERROR_INVALID_EXTERNAL_HANDLE, "VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR" },
    { VK_ERROR_FRAGMENTATION, "VK_ERROR_FRAGMENTATION_EXT" },
    { VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS, "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT" },
    { VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS, "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR" },
    { VK_PIPELINE_COMPILE_REQUIRED_EXT, "VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT" },
    { (VkResult)0x7FFFFFFF, "VK_RESULT_MAX_ENUM" }
};

#define VKCHECK(x)                                                                                      \
    do {                                                                                                \
        VkResult err = x;                                                                               \
        if (err) {                                                                                      \
            SDL_Log("%s:%d Detected Vulkan error: %s", __FILE__, __LINE__, vulkan_errors[err].c_str()); \
            abort();                                                                                    \
        }                                                                                               \
    } while (0)

#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

SDL_Window  *window = {};
VkInstance   instance;
VkSurfaceKHR surface;
VkDevice     device;

struct Queues
{
    bool    seperate_present_queue;
    VkQueue graphics;
    VkQueue compute;
    VkQueue present;
} queues;

int WIDTH  = 800;
int HEIGHT = 600;

extern int main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO);
    auto w_flags = (SDL_WINDOW_VULKAN);
    window       = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, w_flags);





    //
    // Instance
    //

    uint32_t supported_extension_properties_count;
    vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_properties_count, NULL);
    std::vector<VkExtensionProperties> supported_extention_properties(supported_extension_properties_count);
    vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_properties_count, supported_extention_properties.data());

    uint32_t required_extensions_count;
    SDL_Vulkan_GetInstanceExtensions(window, &required_extensions_count, NULL);
    // std::vector<char *> required_instance_extensions(required_extensions_count);
    const char **required_instance_extensions = new const char *[required_extensions_count];
    SDL_Vulkan_GetInstanceExtensions(window, &required_extensions_count, required_instance_extensions);


    // validation layers
    const char *validation_layers[] = {
#if defined(_DEBUG)
        { "VK_LAYER_KHRONOS_validation" },
#endif
        { "VK_LAYER_LUNARG_monitor" },
    };


    VkApplicationInfo application_info  = {};
    application_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pNext              = NULL;
    application_info.pApplicationName   = "name";
    application_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    application_info.pEngineName        = "engine name";
    application_info.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    application_info.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info_instance = {};
#if _DEBUG
    VkValidationFeatureEnableEXT enabled_validation_feature[] = {
        VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
        // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
    };

    VkValidationFeaturesEXT validation_features       = {};
    validation_features.sType                         = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation_features.enabledValidationFeatureCount = ARR_COUNT(enabled_validation_feature);
    validation_features.pEnabledValidationFeatures    = enabled_validation_feature;

    create_info_instance.pNext = &validation_features;
#endif
    create_info_instance.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info_instance.enabledLayerCount       = ARR_COUNT(validation_layers);
    create_info_instance.ppEnabledLayerNames     = validation_layers;
    create_info_instance.flags                   = 0;
    create_info_instance.pApplicationInfo        = &application_info;
    create_info_instance.enabledExtensionCount   = required_extensions_count;
    create_info_instance.ppEnabledExtensionNames = required_instance_extensions;
    VkInstance instance;
    VKCHECK(vkCreateInstance(&create_info_instance, NULL, &instance));





    //
    // Surface
    //

    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        SDL_Log("SDL Failed to create Surface");
    }




    //
    // Physical Device selection
    //
    VkPhysicalDevice physical_device;

    uint32_t physical_device_count = {};
    VKCHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL));
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    VKCHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

    std::vector<VkPhysicalDeviceProperties> physical_device_properties(physical_device_count);
    for (size_t i = 0; i < physical_device_count; i++) {
        vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_properties[i]);
    }

    uint32_t selected_physical_device_idx;
    for (size_t i = 0; i < physical_device_count; i++) {
        auto device_type = physical_device_properties[i].deviceType;
        auto name        = physical_device_properties[i].deviceName;

        if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physical_device = physical_devices[i];
            SDL_Log("GPU-Type [%s][%s]", "DISCRETE_GPU", name);
            selected_physical_device_idx = i;
            break;
        } else if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            physical_device = physical_devices[i];
            SDL_Log("GPU-Type [%s][%s]", "INTEGRATED_GPU", name);
            selected_physical_device_idx = i;
            break;
        } else if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU) {
            physical_device = physical_devices[i];
            SDL_Log("GPU-Type [%s][%s]", "CPU (APU)", name);
            selected_physical_device_idx = i;
            break;
        } else if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
            physical_device = physical_devices[i];
            SDL_Log("GPU-Type [%s][%s]", "VIRTUAL_GPU", name);
            selected_physical_device_idx = i;
            break;
        } else if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER) {
            physical_device = physical_devices[i];
            SDL_Log("GPU-Type [%s][%s]", "UNKNOWN", name);
            selected_physical_device_idx = i;
            break;
        } else {
            physical_device = NULL;
            SDL_Log("No GPU's found"); // todo: dialog box
            abort();
        }
    }

    if (physical_device_properties[selected_physical_device_idx].apiVersion < VK_API_VERSION_1_3) {
        SDL_Log("GPU does not support Vulkan 1.3 Profile");
        abort();
    }





    //
    // Queues
    //

    uint32_t queue_family_properties_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_properties_count, NULL);
    VkQueueFamilyProperties *queue_family_properties = new VkQueueFamilyProperties[queue_family_properties_count];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_properties_count, queue_family_properties);

    if (queue_family_properties == NULL) {
        // some error message
        return 0;
    }

    VkBool32 *queue_idx_supports_present = new VkBool32[queue_family_properties_count];
    for (uint32_t i = 0; i < queue_family_properties_count; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &queue_idx_supports_present[i]);
    }

    uint32_t graphics_queue_family_idx = UINT32_MAX;
    uint32_t compute_queue_family_idx  = UINT32_MAX;
    uint32_t present_queue_family_idx  = UINT32_MAX;

    for (uint32_t i = 0; i < queue_family_properties_count; i++) {
        if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            if (graphics_queue_family_idx == UINT32_MAX)
                graphics_queue_family_idx = i;

        if (queue_idx_supports_present[i] == VK_TRUE) {
            graphics_queue_family_idx = i;
            present_queue_family_idx  = i;
            break;
        }
    }

    for (uint32_t i = 0; i < queue_family_properties_count; i++) {
        if (queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            if (compute_queue_family_idx == UINT32_MAX) {
                compute_queue_family_idx = i;
                break;
            }
    }

    if (present_queue_family_idx == UINT32_MAX) {
        for (uint32_t i = 0; i < queue_family_properties_count; i++) {
            if (queue_idx_supports_present[i]) {
                present_queue_family_idx = i;
                break;
            }
        }
    }

    SDL_Log("Graphics queue family idx: %d\n", graphics_queue_family_idx);
    SDL_Log("Compute  queue family idx: %d\n", compute_queue_family_idx);
    SDL_Log("Present  queue family idx: %d\n", present_queue_family_idx);

    if ((graphics_queue_family_idx & compute_queue_family_idx))
        SDL_Log("Separate Graphics and Compute Queues!\n");


    if ((graphics_queue_family_idx == UINT32_MAX) && (present_queue_family_idx == UINT32_MAX)) {
        // todo(ad): exit on error message
        SDL_LogError(0, "Failed to find Graphics and Present queues");
        abort();
    }

    queues.seperate_present_queue = (graphics_queue_family_idx != present_queue_family_idx);

    const float queue_priorities[] = {
        { 1.0 }
    };





    ///
    /// Device
    ///

    VkPhysicalDeviceFeatures supported_gpu_features = {};
    vkGetPhysicalDeviceFeatures(physical_device, &supported_gpu_features);

    // todo(ad): not used right now
    uint32_t device_properties_count = 0;
    VKCHECK(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &device_properties_count, NULL));
    VkExtensionProperties *device_extension_properties = new VkExtensionProperties[device_properties_count];
    VKCHECK(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &device_properties_count, device_extension_properties));

#if 1
    SDL_Log("Device Extensions count: %d\n", device_properties_count);
    for (size_t i = 0; i < device_properties_count; i++) {
        if (strcmp(device_extension_properties[i].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0)
            SDL_Log("%d: %s\n", device_extension_properties[i].specVersion, device_extension_properties[i].extensionName);
    }
#endif

    const char *enabled_device_extension_names[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        // VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // core in v1.3
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    };

    std::vector<VkDeviceQueueCreateInfo> create_info_device_queues = {};
    VkDeviceQueueCreateInfo              ci_graphics_queue         = {};
    ci_graphics_queue.sType                                        = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    ci_graphics_queue.pNext                                        = NULL;
    ci_graphics_queue.flags                                        = 0;
    ci_graphics_queue.queueFamilyIndex                             = graphics_queue_family_idx;
    ci_graphics_queue.queueCount                                   = 1;
    ci_graphics_queue.pQueuePriorities                             = queue_priorities;
    create_info_device_queues.push_back(ci_graphics_queue);

    VkDeviceQueueCreateInfo ci_compute_queue = {};
    ci_compute_queue.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    ci_compute_queue.pNext                   = NULL;
    ci_compute_queue.flags                   = 0;
    ci_compute_queue.queueFamilyIndex        = compute_queue_family_idx;
    ci_compute_queue.queueCount              = 1;
    ci_compute_queue.pQueuePriorities        = queue_priorities;
    create_info_device_queues.push_back(ci_compute_queue);





    VkPhysicalDeviceVulkan11Features gpu_vulkan_11_features = {};
    gpu_vulkan_11_features.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    gpu_vulkan_11_features.shaderDrawParameters             = VK_TRUE;



    VkPhysicalDeviceSynchronization2Features synchronization2_features = {};
    synchronization2_features.sType                                    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    synchronization2_features.synchronization2                         = VK_TRUE;

    VkPhysicalDeviceRobustness2FeaturesEXT robustness_feature_ext = {};
    robustness_feature_ext.sType                                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
    robustness_feature_ext.nullDescriptor                         = VK_TRUE;
    robustness_feature_ext.pNext                                  = &synchronization2_features;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering = {};
    dynamic_rendering.sType                                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_rendering.dynamicRendering                            = VK_TRUE;
    dynamic_rendering.pNext                                       = &robustness_feature_ext;

    // gpu_vulkan_11_features.pNext = &robustness_feature_ext;

    VkDeviceCreateInfo create_info_device      = {};
    create_info_device.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info_device.pNext                   = &dynamic_rendering;
    create_info_device.flags                   = 0;
    create_info_device.queueCreateInfoCount    = 1;
    create_info_device.pQueueCreateInfos       = create_info_device_queues.data();
    create_info_device.enabledExtensionCount   = ARR_COUNT(enabled_device_extension_names);
    create_info_device.ppEnabledExtensionNames = enabled_device_extension_names;
    create_info_device.pEnabledFeatures        = &supported_gpu_features;

    VKCHECK(vkCreateDevice(physical_device, &create_info_device, NULL, &device));

    if (queues.seperate_present_queue) {
        vkGetDeviceQueue(device, graphics_queue_family_idx, 0, &queues.graphics);
        queues.present = queues.graphics;
    } else {
        vkGetDeviceQueue(device, present_queue_family_idx, 0, &queues.present);
    }

    vkGetDeviceQueue(device, compute_queue_family_idx, 0, &queues.compute);





    SDL_Log("HELLOOOOOO VULKAN!");


    SDL_Event e;
    bool      bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user clicks the X button or alt-f4s
            if (e.type == SDL_QUIT) bQuit = true;
        }
    }
    return 0;

    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
}

#endif