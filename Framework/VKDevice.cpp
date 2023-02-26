#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include "VKDevice.h"


struct DeviceInfo
{
    VkPhysicalDeviceFeatures                   features;
    VkPhysicalDeviceFeatures                   features2;
    VkPhysicalDeviceLimits                     limits;
    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features;
} static device_info;


bool VKDevice::Create(int width, int height)
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        // handle failure
        SDL_Log("FATAL: SDL failed to initialize");
        0;
    }

    this->width = width;
    this->height = height;

    auto w_flags = (SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    window = SDL_CreateWindow("Vulkan Engine Yet To Be", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, w_flags);
    if (!window) {
        SDL_Log("Failed to create window");
        return false;
    }




    //
    // Instance
    //
    {
        uint32_t supported_extension_properties_count;
        vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_properties_count, NULL);
        std::vector<VkExtensionProperties> supported_extention_properties(supported_extension_properties_count);
        vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_properties_count, &supported_extention_properties[0]);

        uint32_t required_extensions_count;
        SDL_Vulkan_GetInstanceExtensions(NULL, &required_extensions_count, NULL);
        // std::vector<char *> required_instance_extensions(required_extensions_count);
        const char** required_instance_extensions = new const char* [required_extensions_count];
        SDL_Vulkan_GetInstanceExtensions(NULL, &required_extensions_count, required_instance_extensions);






        uint32_t layers_count;
        vkEnumerateInstanceLayerProperties(&layers_count, NULL);
        std::vector<VkLayerProperties> layer_properties(layers_count);
        vkEnumerateInstanceLayerProperties(&layers_count, &layer_properties[0]);

        // validation layers
        const char* validation_layers[] = {
            { "VK_LAYER_KHRONOS_validation" },
            { "VK_LAYER_KHRONOS_synchronization2" },
            // { "VK_LAYER_LUNARG_monitor" },
        };


        VkApplicationInfo application_info = {};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pNext = NULL;
        application_info.pApplicationName = "name";
        application_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        application_info.pEngineName = "engine name";
        application_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        application_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo create_info_instance = {};

#if _DEBUG
        VkValidationFeatureEnableEXT enabled_validation_feature[] = {
            VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        };

        VkValidationFeaturesEXT validation_features = {};
        validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validation_features.enabledValidationFeatureCount = ARR_COUNT(enabled_validation_feature);
        validation_features.pEnabledValidationFeatures = enabled_validation_feature;

        create_info_instance.pNext = &validation_features;
        create_info_instance.enabledLayerCount = ARR_COUNT(validation_layers);
        create_info_instance.ppEnabledLayerNames = validation_layers;
#endif

        create_info_instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info_instance.flags = 0;
        create_info_instance.pApplicationInfo = &application_info;
        create_info_instance.enabledExtensionCount = required_extensions_count;
        create_info_instance.ppEnabledExtensionNames = required_instance_extensions;
        VKCHECK(vkCreateInstance(&create_info_instance, NULL, &instance));
    }





    //
    // Surface
    //
    {
        if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
            SDL_Log("SDL Failed to create Surface");
            return false;
        }
    }





    //
    // Physical Device selection
    //
    {
        uint32_t physical_device_count = {};
        VKCHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL));
        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        VKCHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, &physical_devices[0]));

        std::vector<VkPhysicalDeviceProperties> physical_device_properties(physical_device_count);
        for (size_t i = 0; i < physical_device_count; i++) {
            vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_properties[i]);
        }

        struct tmpGpu
        {
            VkPhysicalDeviceType type;
            const char* type_name;
            const char* name;
            uint32_t             index;
        };
        std::vector<tmpGpu> available_gpus(physical_device_count);

        uint32_t selected_physical_device_idx = -1;
        for (uint32_t i = 0; i < physical_device_count; i++) {
            auto device_type = physical_device_properties[i].deviceType;
            auto name = physical_device_properties[i].deviceName;

            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                available_gpus[i].type_name = "DISCRETE_GPU";
                available_gpus[i].index = i;
            }
            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                available_gpus[i].type_name = "INTEGRATED_GPU";
                available_gpus[i].index = i;
            }
            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU) {
                available_gpus[i].type_name = "CPU (APU)";
                available_gpus[i].index = i;
            }
            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
                available_gpus[i].type_name = "VIRTUAL_GPU";
                available_gpus[i].index = i;
            }
            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER) {

                available_gpus[i].type_name = "UNKNOWN";
                available_gpus[i].index = i;
            }

            available_gpus[i].type = device_type;
            available_gpus[i].name = name;
        }

#if PROMPT_GPU_SELECTION_AT_STARTUP
        uint32_t selection = -1;
        for (size_t i = 0; i < available_gpus.size(); i++) {
            SDL_Log("%d) (%s) %s", i, available_gpus[i].type_name, available_gpus[i].name);
        }

        do {
            SDL_Log("Select desired GPU to continue:");
            scanf("%d", &selection);
        } while (selection < 0 || selection > available_gpus.size() - 1);

        selected_physical_device_idx = available_gpus[selection].index;

#else
        for (size_t i = 0; i < available_gpus.size(); i++) {
            if (available_gpus[i].type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                selected_physical_device_idx = available_gpus[i].index;
                break;
            }
        }

        if (selected_physical_device_idx == -1) {
            for (size_t i = 0; i < available_gpus.size(); i++) {
                if (available_gpus[i].type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                    selected_physical_device_idx = available_gpus[i].index;
            }
        }

#endif
        physical_device = physical_devices[selected_physical_device_idx];
        SDL_Log("Selected GPU:[%s][%s]", available_gpus[selected_physical_device_idx].type_name, available_gpus[selected_physical_device_idx].name);

        if (physical_device_properties[selected_physical_device_idx].apiVersion < VK_API_VERSION_1_3) {
            SDL_Log("GPU does not support Vulkan 1.3 Profile, consider updating drivers");
            return false;
        }




        device_info.limits = physical_device_properties[selected_physical_device_idx].limits;

        device_info.descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

        VkPhysicalDeviceFeatures2 physical_device_features2{};
        physical_device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        physical_device_features2.pNext = &device_info.descriptor_indexing_features;

        vkGetPhysicalDeviceFeatures2(physical_device, &physical_device_features2);
        vkGetPhysicalDeviceFeatures(physical_device, &device_info.features);

        device_info.features2 = physical_device_features2.features;
    }





    //
    // Queues
    //
    {
        uint32_t queue_family_properties_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_properties_count, NULL);
        VkQueueFamilyProperties* queue_family_properties = new VkQueueFamilyProperties[queue_family_properties_count];
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_properties_count, queue_family_properties);

        if (queue_family_properties == NULL) {
            // some error message
            //return 0;
        }

        VkBool32* queue_idx_supports_present = new VkBool32[queue_family_properties_count];
        for (uint32_t i = 0; i < queue_family_properties_count; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &queue_idx_supports_present[i]);
        }

        uint32_t graphics_queue_family_idx = UINT32_MAX;
        uint32_t compute_queue_family_idx = UINT32_MAX;
        uint32_t present_queue_family_idx = UINT32_MAX;

        for (uint32_t i = 0; i < queue_family_properties_count; i++) {
            if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                if (graphics_queue_family_idx == UINT32_MAX)
                    graphics_queue_family_idx = i;

            if (queue_idx_supports_present[i] == VK_TRUE) {
                graphics_queue_family_idx = i;
                present_queue_family_idx = i;
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
            SDL_LogError(0, "Failed to find Graphics and Present gQueues");
            return false;
        }

        queues.seperate_present_queue = (graphics_queue_family_idx != present_queue_family_idx);

        queues.graphics_queue_family_idx = graphics_queue_family_idx;
        queues.compute_queue_family_idx = compute_queue_family_idx;
        queues.present_queue_family_idx = present_queue_family_idx;
    }





    ///
    /// Device
    ///
    {

        // todo(ad): not used right now
        uint32_t device_properties_count = 0;
        VKCHECK(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &device_properties_count, NULL));
        VkExtensionProperties* device_extension_properties = new VkExtensionProperties[device_properties_count];
        VKCHECK(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &device_properties_count, device_extension_properties));

#if 0
        SDL_Log("Device Extensions count: %d\n", device_properties_count);
        for (size_t i = 0; i < device_properties_count; i++) {
            if (strcmp(device_extension_properties[i].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0)
                SDL_Log("%d: %s\n", device_extension_properties[i].specVersion, device_extension_properties[i].extensionName);
        }
#endif

        const float queue_priorities[] = {
            { 1.0 }
        };

        std::vector<VkDeviceQueueCreateInfo> create_info_device_queues{};

        VkDeviceQueueCreateInfo ci_graphics_queue{};
        ci_graphics_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        ci_graphics_queue.pNext = NULL;
        ci_graphics_queue.flags = 0;
        ci_graphics_queue.queueFamilyIndex = queues.graphics_queue_family_idx;
        ci_graphics_queue.queueCount = 1;
        ci_graphics_queue.pQueuePriorities = queue_priorities;
        create_info_device_queues.push_back(ci_graphics_queue);

        VkDeviceQueueCreateInfo ci_compute_queue{};
        ci_compute_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        ci_compute_queue.pNext = NULL;
        ci_compute_queue.flags = 0;
        ci_compute_queue.queueFamilyIndex = queues.compute_queue_family_idx;
        ci_compute_queue.queueCount = 1;
        ci_compute_queue.pQueuePriorities = queue_priorities;
        create_info_device_queues.push_back(ci_compute_queue);


        VkPhysicalDeviceFeatures enabled_physical_device_features{};
        enabled_physical_device_features.samplerAnisotropy = VK_TRUE;


        // VkPhysicalDeviceVulkan11Features gpu_vulkan_11_features {};
        // gpu_vulkan_11_features.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        // gpu_vulkan_11_features.shaderDrawParameters = VK_TRUE;

        // if (device_info._descriptor_indexing_features.runtimeDescriptorArray && device_info._descriptor_indexing_features.descriptorBindingPartiallyBound)


        VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
        descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        descriptor_indexing_features.runtimeDescriptorArray = VK_TRUE;
        descriptor_indexing_features.descriptorBindingPartiallyBound = VK_TRUE;

        VkPhysicalDeviceSynchronization2Features synchronization2_features{};
        synchronization2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        synchronization2_features.pNext = &descriptor_indexing_features;
        synchronization2_features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceRobustness2FeaturesEXT robustness_feature_ext{};
        robustness_feature_ext.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
        robustness_feature_ext.pNext = &synchronization2_features;
        robustness_feature_ext.nullDescriptor = VK_TRUE;

        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{};
        dynamic_rendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynamic_rendering.pNext = &robustness_feature_ext;
        dynamic_rendering.dynamicRendering = VK_TRUE;

        // gpu_vulkan_11_features.pNext = &robustness_feature_ext;
        const char* enabled_device_extension_names[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
            // VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, // core in 1.3
            // VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // core in 1.3
            // VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        };

        VkDeviceCreateInfo create_info_device = {};
        create_info_device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info_device.pNext = &dynamic_rendering; // seens to be required even if it's core in 1.3
        create_info_device.flags = 0;
        create_info_device.queueCreateInfoCount = 1;
        create_info_device.pQueueCreateInfos = &create_info_device_queues[0];
        create_info_device.enabledExtensionCount = ARR_COUNT(enabled_device_extension_names);
        create_info_device.ppEnabledExtensionNames = enabled_device_extension_names;
        create_info_device.pEnabledFeatures = &enabled_physical_device_features;

        VKCHECK(vkCreateDevice(physical_device, &create_info_device, NULL, &device));

        if (queues.seperate_present_queue) {
            vkGetDeviceQueue(device, queues.present_queue_family_idx, 0, &queues.present);
        }
        else {
            vkGetDeviceQueue(device, queues.graphics_queue_family_idx, 0, &queues.graphics);
            queues.present = queues.graphics;
        }
        vkGetDeviceQueue(device, queues.compute_queue_family_idx, 0, &queues.compute);
    }

    return true;
}