#define VMA_IMPLEMENTATION

#if !defined(CGLTF_IMPLEMENTATION)
#define CGLTF_IMPLEMENTATION
#endif

#if !defined(STB_IMAGE_IMPLEMENTATION)
#define STB_IMAGE_IMPLEMENTATION
#endif

#include "vk_backend.h"

#if defined(VULKAN) && !defined(GL)


#include "utils.h"
#include "vk_Mesh.h"
#include "Camera.h"

Camera camera;


extern int
main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_EVERYTHING);
    auto w_flags = (SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    gWindow      = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, w_flags);





    //
    // Instance
    //
    {
        uint32_t supported_extension_properties_count;
        vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_properties_count, NULL);
        std::vector<VkExtensionProperties> supported_extention_properties(supported_extension_properties_count);
        vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_properties_count, &supported_extention_properties[0]);

        uint32_t required_extensions_count;
        SDL_Vulkan_GetInstanceExtensions(gWindow, &required_extensions_count, NULL);
        // std::vector<char *> required_instance_extensions(required_extensions_count);
        const char **required_instance_extensions = new const char *[required_extensions_count];
        SDL_Vulkan_GetInstanceExtensions(gWindow, &required_extensions_count, required_instance_extensions);


        uint32_t layers_count;
        vkEnumerateInstanceLayerProperties(&layers_count, NULL);
        std::vector<VkLayerProperties> layer_properties(layers_count);
        vkEnumerateInstanceLayerProperties(&layers_count, &layer_properties[0]);

        // validation layers
        const char *validation_layers[] = {
            { "VK_LAYER_KHRONOS_validation" },
            { "VK_LAYER_KHRONOS_synchronization2" },
            // { "VK_LAYER_LUNARG_monitor" },
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
            VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        };

        VkValidationFeaturesEXT validation_features       = {};
        validation_features.sType                         = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validation_features.enabledValidationFeatureCount = ARR_COUNT(enabled_validation_feature);
        validation_features.pEnabledValidationFeatures    = enabled_validation_feature;

        create_info_instance.pNext               = &validation_features;
        create_info_instance.enabledLayerCount   = ARR_COUNT(validation_layers);
        create_info_instance.ppEnabledLayerNames = validation_layers;
#endif
        create_info_instance.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info_instance.flags                   = 0;
        create_info_instance.pApplicationInfo        = &application_info;
        create_info_instance.enabledExtensionCount   = required_extensions_count;
        create_info_instance.ppEnabledExtensionNames = required_instance_extensions;
        VKCHECK(vkCreateInstance(&create_info_instance, NULL, &gInstance));
    }





    //
    // Surface
    //
    {
        if (!SDL_Vulkan_CreateSurface(gWindow, gInstance, &gSurface)) {
            SDL_Log("SDL Failed to create Surface");
        }
    }





    //
    // Physical Device selection
    //
    {
        uint32_t physical_device_count = {};
        VKCHECK(vkEnumeratePhysicalDevices(gInstance, &physical_device_count, NULL));
        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        VKCHECK(vkEnumeratePhysicalDevices(gInstance, &physical_device_count, &physical_devices[0]));

        std::vector<VkPhysicalDeviceProperties> physical_device_properties(physical_device_count);
        for (size_t i = 0; i < physical_device_count; i++) {
            vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_properties[i]);
        }

        struct GPU
        {
            VkPhysicalDeviceType type;
            const char          *type_name;
            const char          *name;
            uint32_t             index;
        };
        std::vector<GPU> available_gpus(physical_device_count);

        uint32_t selected_physical_device_idx;
        for (size_t i = 0; i < physical_device_count; i++) {
            auto device_type = physical_device_properties[i].deviceType;
            auto name        = physical_device_properties[i].deviceName;

            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                available_gpus[i].type_name = "DISCRETE_GPU";
                available_gpus[i].index     = i;
            }
            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                available_gpus[i].type_name = "INTEGRATED_GPU";
                available_gpus[i].index     = i;
            }
            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU) {
                available_gpus[i].type_name = "CPU (APU)";
                available_gpus[i].index     = i;
            }
            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
                available_gpus[i].type_name = "VIRTUAL_GPU";
                available_gpus[i].index     = i;
            }
            if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER) {

                available_gpus[i].type_name = "UNKNOWN";
                available_gpus[i].index     = i;
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
            if (available_gpus[i].type = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                selected_physical_device_idx = available_gpus[i].index;
                break;
            } else if (available_gpus[i].type = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                selected_physical_device_idx = available_gpus[i].index;
        }

#endif
        gPhysical_device = physical_devices[selected_physical_device_idx];
        SDL_Log("Selected GPU:[%s][%s]", available_gpus[selected_physical_device_idx].type_name, available_gpus[selected_physical_device_idx].name);

        if (physical_device_properties[selected_physical_device_idx].apiVersion < VK_API_VERSION_1_3) {
            SDL_Log("GPU does not support Vulkan 1.3 Profile, consider updating drivers");
        }
    }





    //
    // Queues
    //
    {
        uint32_t queue_family_properties_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gPhysical_device, &queue_family_properties_count, NULL);
        VkQueueFamilyProperties *queue_family_properties = new VkQueueFamilyProperties[queue_family_properties_count];
        vkGetPhysicalDeviceQueueFamilyProperties(gPhysical_device, &queue_family_properties_count, queue_family_properties);

        if (queue_family_properties == NULL) {
            // some error message
            return 0;
        }

        VkBool32 *queue_idx_supports_present = new VkBool32[queue_family_properties_count];
        for (uint32_t i = 0; i < queue_family_properties_count; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(gPhysical_device, i, gSurface, &queue_idx_supports_present[i]);
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
            SDL_LogError(0, "Failed to find Graphics and Present gQueues");
            abort();
        }

        gQueues._seperate_present_queue = (graphics_queue_family_idx != present_queue_family_idx);

        gQueues._graphics_queue_family_idx = graphics_queue_family_idx;
        gQueues._compute_queue_family_idx  = compute_queue_family_idx;
        gQueues._present_queue_family_idx  = present_queue_family_idx;
    }





    ///
    /// Device
    ///
    {
        VkPhysicalDeviceFeatures supported_gpu_features = {};
        vkGetPhysicalDeviceFeatures(gPhysical_device, &supported_gpu_features);

        // todo(ad): not used right now
        uint32_t device_properties_count = 0;
        VKCHECK(vkEnumerateDeviceExtensionProperties(gPhysical_device, NULL, &device_properties_count, NULL));
        VkExtensionProperties *device_extension_properties = new VkExtensionProperties[device_properties_count];
        VKCHECK(vkEnumerateDeviceExtensionProperties(gPhysical_device, NULL, &device_properties_count, device_extension_properties));

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


        std::vector<VkDeviceQueueCreateInfo> create_info_device_queues = {};
        VkDeviceQueueCreateInfo              ci_graphics_queue         = {};
        ci_graphics_queue.sType                                        = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        ci_graphics_queue.pNext                                        = NULL;
        ci_graphics_queue.flags                                        = 0;
        ci_graphics_queue.queueFamilyIndex                             = gQueues._graphics_queue_family_idx;
        ci_graphics_queue.queueCount                                   = 1;
        ci_graphics_queue.pQueuePriorities                             = queue_priorities;
        create_info_device_queues.push_back(ci_graphics_queue);

        VkDeviceQueueCreateInfo ci_compute_queue = {};
        ci_compute_queue.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        ci_compute_queue.pNext                   = NULL;
        ci_compute_queue.flags                   = 0;
        ci_compute_queue.queueFamilyIndex        = gQueues._compute_queue_family_idx;
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
        const char *enabled_device_extension_names[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            // VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            // VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // core in v1.3
            // VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        };

        VkDeviceCreateInfo create_info_device      = {};
        create_info_device.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info_device.pNext                   = &dynamic_rendering; // seens to be required even if it's core in 1.3
        create_info_device.flags                   = 0;
        create_info_device.queueCreateInfoCount    = 1;
        create_info_device.pQueueCreateInfos       = &create_info_device_queues[0];
        create_info_device.enabledExtensionCount   = ARR_COUNT(enabled_device_extension_names);
        create_info_device.ppEnabledExtensionNames = enabled_device_extension_names;
        create_info_device.pEnabledFeatures        = &supported_gpu_features;

        VKCHECK(vkCreateDevice(gPhysical_device, &create_info_device, NULL, &gDevice));

        if (gQueues._seperate_present_queue) {
            vkGetDeviceQueue(gDevice, gQueues._present_queue_family_idx, 0, &gQueues._present);
        } else {
            vkGetDeviceQueue(gDevice, gQueues._graphics_queue_family_idx, 0, &gQueues._graphics);
            gQueues._present = gQueues._graphics;
        }
        vkGetDeviceQueue(gDevice, gQueues._compute_queue_family_idx, 0, &gQueues._compute);
    }





    //
    // Swapchain
    //
    {
        gSwapchain.Create(gDevice, gPhysical_device, gSurface, VK_NULL_HANDLE);
        gFrames.resize(gSwapchain._image_count);

        SDL_Log("HELLOOOOOO VULKAN!");
    }





    //
    // Command Pools
    //
    {
        VkCommandPoolCreateInfo command_pool_ci = {};
        command_pool_ci.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_ci.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_ci.queueFamilyIndex        = gQueues._graphics_queue_family_idx;
        VKCHECK(vkCreateCommandPool(gDevice, &command_pool_ci, NULL, &gGraphics_command_pool));



        //
        // Per frame objects
        //
        for (size_t i = 0; i < gSwapchain._image_count; i++) {
            VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
            command_buffer_allocate_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_buffer_allocate_info.commandPool                 = gGraphics_command_pool;
            command_buffer_allocate_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_buffer_allocate_info.commandBufferCount          = 1;
            VKCHECK(vkAllocateCommandBuffers(gDevice, &command_buffer_allocate_info, &gFrames[i].graphics_command_buffer));

            VkFenceCreateInfo fence_ci = {};
            fence_ci.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_ci.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
            VKCHECK(vkCreateFence(gDevice, &fence_ci, NULL, &gFrames[i].render_fence));

            VkSemaphoreCreateInfo semaphoreCreateInfo = {};
            semaphoreCreateInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreCreateInfo.pNext                 = NULL;
            semaphoreCreateInfo.flags                 = 0;

            VKCHECK(vkCreateSemaphore(gDevice, &semaphoreCreateInfo, NULL, &gFrames[i].present_semaphore));
            VKCHECK(vkCreateSemaphore(gDevice, &semaphoreCreateInfo, NULL, &gFrames[i].render_semaphore));
        }
    }





    //
    // VMA allocator
    //
    {
        VmaAllocatorCreateInfo vma_allocator_ci {};
        vma_allocator_ci.instance       = gInstance;
        vma_allocator_ci.device         = gDevice;
        vma_allocator_ci.physicalDevice = gPhysical_device;

        vmaCreateAllocator(&vma_allocator_ci, &gAllocator);
    }



    //
    // Depth image
    //

    // VkImage depth_image;


    //
    // Descriptor pool
    //
    {
        // we are creating a pool that can potentially hold 16 descriptor sets that hold swapchain.image_count number of uniform buffer descriptors
        VkDescriptorPool     descriptor_pool;
        VkDescriptorPoolSize descriptor_pool_sizes[1] {};
        descriptor_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_pool_sizes[0].descriptorCount = gSwapchain._image_count; // total amount of a given descriptor type accross all sets

        VkDescriptorPoolCreateInfo descriptor_pool_ci {};
        descriptor_pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool_ci.flags         = 0;
        descriptor_pool_ci.maxSets       = gSwapchain._image_count;
        descriptor_pool_ci.poolSizeCount = ARR_COUNT(descriptor_pool_sizes); // 1 PoolSize = TYPE count of descriptors
        descriptor_pool_ci.pPoolSizes    = descriptor_pool_sizes;
        VKCHECK(vkCreateDescriptorPool(gDevice, &descriptor_pool_ci, NULL, &descriptor_pool));


        //
        // Descriptor set layouts
        //

        // set 0 bindings
        // the layout for the set at binding 0 that will reference a MVP uniform blok
        std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings(1);
        set_layout_bindings[0].binding         = 0;
        set_layout_bindings[0].descriptorCount = 1; // 1 instance of MVP block for this binding
        set_layout_bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        // set 0
        gDescriptor_set_layouts.resize(gSwapchain._image_count);
        gDescriptor_sets.resize(gSwapchain._image_count);


        for (size_t i = 0; i < gSwapchain._image_count; i++) {
            VkDescriptorSetLayoutCreateInfo set_layouts_ci {};
            set_layouts_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            set_layouts_ci.pNext        = NULL;
            set_layouts_ci.flags        = 0;
            set_layouts_ci.bindingCount = set_layout_bindings.size();
            set_layouts_ci.pBindings    = &set_layout_bindings[0];

            // we need <frame_count> descriptor sets that each reference 1 MVP uniform block for each frame in flight
            VKCHECK(vkCreateDescriptorSetLayout(gDevice, &set_layouts_ci, NULL, &gDescriptor_set_layouts[i]));
        }


        VkDescriptorSetAllocateInfo descriptor_set_alloc_info {};
        descriptor_set_alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_alloc_info.descriptorPool     = descriptor_pool;
        descriptor_set_alloc_info.descriptorSetCount = gDescriptor_sets.size();
        descriptor_set_alloc_info.pSetLayouts        = &gDescriptor_set_layouts[0];
        VKCHECK(vkAllocateDescriptorSets(gDevice, &descriptor_set_alloc_info, &gDescriptor_sets[0]));
    }





    //
    // Shader uniforms & Descriptor set updates/writes
    //
    std::vector<void *> mapped_ubo_ptrs(gSwapchain._image_count);
    {

        for (size_t i = 0; i < gSwapchain._image_count; i++) {

            {
                CreateBuffer(&gFrames[i].UBO.handle, sizeof(glm::mat4) * 3, &gFrames[i].UBO.vma_allocation,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

                vmaMapMemory(gAllocator, gFrames[i].UBO.vma_allocation, &mapped_ubo_ptrs[i]);
            }

            {
                VkDescriptorBufferInfo buffer_info {};
                buffer_info.buffer = gFrames[i].UBO.handle;
                buffer_info.offset = 0;
                buffer_info.range  = VK_WHOLE_SIZE;

                VkWriteDescriptorSet descriptor_writes {};
                descriptor_writes.sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor_writes.dstSet     = gDescriptor_sets[i];
                descriptor_writes.dstBinding = 0;
                descriptor_writes.dstArrayElement;
                descriptor_writes.descriptorCount = 1;
                descriptor_writes.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptor_writes.pImageInfo;
                descriptor_writes.pBufferInfo = &buffer_info;
                descriptor_writes.pTexelBufferView;
                vkUpdateDescriptorSets(gDevice, 1, &descriptor_writes, 0, NULL);
            }
        }
    }




    //
    // Pipelines
    //
    CreateGraphicsPipeline(gDevice, &gDefault_graphics_pipeline);


    camera.CameraCreate({ 16, 20, 44 }, 45.f, WIDTH / (float)HEIGHT, .8f, 4000.f);
    camera._pitch  = -20;
    gActive_camera = &camera;


    glm::vec3 translation = { 0, 0, 0 };
    glm::vec3 rotation    = { 0, 0, 0 };
    glm::vec3 scale       = { 10, 10, 10 };

    SkinnedModel skinned_model;
    skinned_model.Create("assets/warrior/warrior.gltf");
    glm::mat4 model = glm::mat4(1);

    model = glm::translate(model, translation)
        * glm::rotate(model, (glm::radians(rotation.z)), glm::vec3(0, 0, 1))
        * glm::rotate(model, (glm::radians(rotation.y)), glm::vec3(0, 1, 0))
        * glm::rotate(model, (glm::radians(rotation.x)), glm::vec3(1, 0, 0))
        * glm::scale(model, glm::vec3(scale));




    bool     bQuit            = false;
    uint32_t frame_in_flight  = 0;
    bool     window_minimized = false;

    uint64_t lastCycleCount = __rdtsc();
    uint64_t lastCounter    = SDL_GetPerformanceCounter();
    uint64_t perfFrequency  = SDL_GetPerformanceFrequency();
    float    dt             = 0;


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
                        if (key == SDLK_f) SDL_SetWindowFullscreen(gWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
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



        gActive_camera->CameraUpdate(&input, dt);

        VkResult result;

        VkCommandBuffer graphics_cmd_buffer = gFrames[frame_in_flight].graphics_command_buffer;
        // todo: semaphore & gFrames

        {
            // wait for the gFrames[frame_in_flight].render_fence to be unsignaled by [who?] before acquiring
            VKCHECK(vkWaitForFences(gDevice, 1, &gFrames[frame_in_flight].render_fence, VK_TRUE, SECONDS(1)));
            VKCHECK(vkResetFences(gDevice, 1, &gFrames[frame_in_flight].render_fence));
            VKCHECK(result = vkAcquireNextImageKHR(gDevice, gSwapchain._handle, SECONDS(1), 0, gFrames[frame_in_flight].render_fence, &gFrames[frame_in_flight].image_idx));
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                gSwapchain.Create(gDevice, gPhysical_device, gSurface, gSwapchain._handle);
            }
        }

        {
            // VKCHECK(vkResetFences(gDevice, 1, &gFrames[frame_in_flight].render_fence));
            VKCHECK(vkResetCommandBuffer(graphics_cmd_buffer, 0));

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VKCHECK(vkBeginCommandBuffer(graphics_cmd_buffer, &begin_info));





            //
            //  Bindings
            //

            UBO_data.projection = gActive_camera->_projection;
            UBO_data.view       = gActive_camera->_view;
            UBO_data.model      = model;
            UBO_data.projection[1][1] *= -1;


            memcpy(mapped_ubo_ptrs[frame_in_flight], &UBO_data, sizeof(UBOData));

            vkCmdBindDescriptorSets(graphics_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 0, 1, &gDescriptor_sets[frame_in_flight], 0, NULL);
            vkCmdBindPipeline(graphics_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline);

            VkViewport viewport {};
            viewport.minDepth = 0;
            viewport.maxDepth = 1;
            viewport.width    = (float)WIDTH;
            viewport.height   = (float)HEIGHT;

            VkRect2D scissor {};
            scissor.extent.width  = WIDTH;
            scissor.extent.height = HEIGHT;


            vkCmdSetViewport(graphics_cmd_buffer, 0, 1, &viewport);
            vkCmdSetScissor(graphics_cmd_buffer, 0, 1, &scissor);





            //
            // Swapchain image layout transition for Rendering
            //
            {
                VkImageMemoryBarrier2 image_memory_barrier_before_rendering = {};
                image_memory_barrier_before_rendering.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

                image_memory_barrier_before_rendering.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                // image_memory_barrier_before_rendering.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                image_memory_barrier_before_rendering.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                image_memory_barrier_before_rendering.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                image_memory_barrier_before_rendering.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
                image_memory_barrier_before_rendering.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                image_memory_barrier_before_rendering.image                       = gSwapchain._images[gFrames[frame_in_flight].image_idx];
                image_memory_barrier_before_rendering.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_memory_barrier_before_rendering.subresourceRange.layerCount = 1;
                image_memory_barrier_before_rendering.subresourceRange.levelCount = 1;

                VkDependencyInfo dependency_info        = {};
                dependency_info.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependency_info.imageMemoryBarrierCount = 1;
                dependency_info.pImageMemoryBarriers    = &image_memory_barrier_before_rendering;
                vkCmdPipelineBarrier2(graphics_cmd_buffer, &dependency_info);
            }



            ////////////////////////////////////////////
            // Rendering
            //

            {
                VkClearValue color_clear_value = {};
                color_clear_value.color        = { 0.f, 1.f, 0.f, 1.f };

                VkRenderingAttachmentInfo color_attachment_info = {};
                color_attachment_info.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                color_attachment_info.imageView                 = gSwapchain._image_views[gFrames[frame_in_flight].image_idx];
                color_attachment_info.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
                color_attachment_info.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
                color_attachment_info.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
                color_attachment_info.clearValue                = color_clear_value;


                VkClearValue depth_clear_value {};
                depth_clear_value.depthStencil.depth = { .5 };

                VkRenderingAttachmentInfo depth_attachment_info = {};
                depth_attachment_info.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                // depth_attachment_info.imageView                 = gSwapchain._image_views[gFrames[frame_in_flight].image_idx];
                depth_attachment_info.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
                depth_attachment_info.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depth_attachment_info.storeOp                   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depth_attachment_info.clearValue                = depth_clear_value;

                VkRenderingInfo rendering_info      = {};
                rendering_info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
                rendering_info.renderArea.extent    = { WIDTH, HEIGHT };
                rendering_info.layerCount           = 1;
                rendering_info.colorAttachmentCount = 1;
                rendering_info.pColorAttachments    = &color_attachment_info;
                rendering_info.pDepthAttachment     = &depth_attachment_info;
                rendering_info.pStencilAttachment; // todo

                {
                    vkCmdBeginRendering(graphics_cmd_buffer, &rendering_info);

                    skinned_model.Draw(graphics_cmd_buffer);

                    vkCmdEndRendering(graphics_cmd_buffer);
                }
            }




            //
            // Swapchain image layout transition for presenting
            //
            {
                VkImageMemoryBarrier2 image_memory_barrier_before_presenting = {};
                image_memory_barrier_before_presenting.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

                image_memory_barrier_before_presenting.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                image_memory_barrier_before_presenting.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                image_memory_barrier_before_presenting.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                // image_memory_barrier_before_presenting.dstAccessMask         = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

                image_memory_barrier_before_presenting.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                image_memory_barrier_before_presenting.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

                image_memory_barrier_before_presenting.image                       = gSwapchain._images[gFrames[frame_in_flight].image_idx];
                image_memory_barrier_before_presenting.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_memory_barrier_before_presenting.subresourceRange.layerCount = 1;
                image_memory_barrier_before_presenting.subresourceRange.levelCount = 1;

                VkDependencyInfo dependency_info        = {};
                dependency_info.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependency_info.imageMemoryBarrierCount = 1;
                dependency_info.pImageMemoryBarriers    = &image_memory_barrier_before_presenting;
                vkCmdPipelineBarrier2(graphics_cmd_buffer, &dependency_info);
            }




            VKCHECK(vkEndCommandBuffer(graphics_cmd_buffer));
        }



        //
        // Command Buffer submission
        //

        {
            VkCommandBufferSubmitInfo command_buffer_submit_info = {};
            command_buffer_submit_info.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_info.pNext                     = NULL;
            command_buffer_submit_info.commandBuffer             = graphics_cmd_buffer;
            command_buffer_submit_info.deviceMask;

            VkSemaphoreSubmitInfo wait_semephore_info = {};
            wait_semephore_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            wait_semephore_info.semaphore             = gFrames[frame_in_flight].present_semaphore;
            wait_semephore_info.value;
            wait_semephore_info.stageMask = VK_PIPELINE_STAGE_NONE;
            wait_semephore_info.deviceIndex;

            VkSemaphoreSubmitInfo signal_semephore_info = {};
            signal_semephore_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signal_semephore_info.semaphore             = gFrames[frame_in_flight].render_semaphore;
            signal_semephore_info.value;
            signal_semephore_info.stageMask = VK_PIPELINE_STAGE_NONE;
            signal_semephore_info.deviceIndex;

            VkSubmitInfo2 submit_info = {};
            submit_info.sType         = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submit_info.flags;
            submit_info.waitSemaphoreInfoCount = 0;
            // submit_info.pWaitSemaphoreInfos      = &wait_semephore_info; // wait for image to be presented before rendering again
            submit_info.commandBufferInfoCount   = 1;
            submit_info.pCommandBufferInfos      = &command_buffer_submit_info;
            submit_info.signalSemaphoreInfoCount = 1;
            submit_info.pSignalSemaphoreInfos    = &signal_semephore_info;

            VKCHECK(vkWaitForFences(gDevice, 1, &gFrames[frame_in_flight].render_fence, VK_TRUE, SECONDS(1)));
            VKCHECK(vkResetFences(gDevice, 1, &gFrames[frame_in_flight].render_fence));

            VKCHECK(vkQueueSubmit2(gQueues._graphics, 1, &submit_info, gFrames[frame_in_flight].render_fence));
        }


        {
            //
            // Presenting
            //
            VkPresentInfoKHR present_info   = {};
            present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores    = &gFrames[frame_in_flight].render_semaphore; // the semaphore to wait upon before presenting
            present_info.swapchainCount     = 1;
            present_info.pSwapchains        = &gSwapchain._handle;
            present_info.pImageIndices      = &gFrames[frame_in_flight].image_idx;
            present_info.pResults;

            VKCHECK(result = vkQueuePresentKHR(gQueues._present, &present_info));
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                gSwapchain.Create(gDevice, gPhysical_device, gSurface, gSwapchain._handle);
            }
        }

        ++frame_in_flight;
        frame_in_flight = frame_in_flight % gSwapchain._image_count;





        uint64_t endCounter = SDL_GetPerformanceCounter();
        uint64_t frameDelta = endCounter - lastCounter;

        dt = frameDelta / (float)perfFrequency;


        uint64_t endCycleCount   = __rdtsc();
        uint64_t cycleDelta      = endCycleCount - lastCycleCount;
        float    MCyclesPerFrame = (float)(cycleDelta / (float)(1000 * 1000));
        lastCounter              = endCounter;
        lastCycleCount           = endCycleCount;



        static float acc = 0;
        if ((acc += dt) > 1) {
            static char buffer[256];
            sprintf_s(buffer, sizeof(buffer), "%.2f fps | %.2f ms | %.2f MCycles | CPU: %.2f Mhz",
                1 / dt,
                dt * 1000,
                MCyclesPerFrame,
                MCyclesPerFrame / dt);

            SDL_SetWindowTitle(gWindow, buffer);
            acc = 0;
        }
    }
    return 0;

    vkDestroySurfaceKHR(gInstance, gSurface, NULL);
    vkDestroyInstance(gInstance, NULL);
}
#endif