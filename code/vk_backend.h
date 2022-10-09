#include "backend.h"

#if defined(VULKAN) && !defined(GL)
#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

// #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <GLM/glm.hpp>
#include <GLM/gtx/quaternion.hpp>
#include <GLM/gtc/matrix_transform.hpp>

#include <stb_image.h>
#include <cgltf.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>





#define PROMPT_GPU_SELECTION_AT_STARTUP 0




#define SECONDS(value) (1000000000 * value)
#define ARR_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

struct Camera;

struct Input
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool Q;
    bool E;

    struct Mouse
    {
        int32_t xrel;
        int32_t yrel;
        bool    left;
        bool    right;
    } mouse;
} input = {};

Camera *gActive_camera;

////////////////////////////
// Vulkan specific

static std::unordered_map<VkResult, std::string>
    vulkan_errors = {
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

#if defined(_DEBUG)
#define VKCHECK(x)                                                                                      \
    do {                                                                                                \
        VkResult err = x;                                                                               \
        if (err) {                                                                                      \
            SDL_Log("%s:%d Detected Vulkan error: %s", __FILE__, __LINE__, vulkan_errors[err].c_str()); \
        }                                                                                               \
    } while (0)
#else
#define VKCHECK(x) x
#endif

///////////////////////////
// Types

struct Image
{
    VkImage       handle;
    VmaAllocation vma_allocation;
};


struct Buffer
{
    VkBuffer      handle;
    VmaAllocation vma_allocation;
};


struct UBOData
{
    alignas(16) glm::mat4 projection;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 model;
} UBO_data;

////////////////////////////
// Runtime data

// VK_PRESENT_MODE_FIFO_KHR; // widest support / VSYNC
// VK_PRESENT_MODE_IMMEDIATE_KHR; // present as fast as possible, high tearing chance
// VK_PRESENT_MODE_MAILBOX_KHR; // present as fast as possible, high tearing chance
VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

SDL_Window      *gWindow = {};
VkInstance       gInstance;
VkSurfaceKHR     gSurface;
VkPhysicalDevice gPhysical_device;
VkDevice         gDevice;
VmaAllocator     gAllocator;

struct Queues
{
    bool     _seperate_present_queue;
    uint32_t _graphics_queue_family_idx;
    uint32_t _compute_queue_family_idx;
    uint32_t _present_queue_family_idx;
    VkQueue  _graphics;
    VkQueue  _compute;
    VkQueue  _present;
} gQueues;

VkCommandPool   gGraphics_command_pool;
VkCommandBuffer gGraphics_command_buffer;

struct Swapchain
{
    VkSwapchainKHR _handle = VK_NULL_HANDLE;

    // note: do these always come together?
    std::vector<VkImage>     _images      = {};
    std::vector<VkImageView> _image_views = {};
    VkFormat                 _format;
    Image                    _depth_image      = {};
    VkImageView              _depth_image_view = {};
    uint32_t                 _image_count      = {};
    void                     Create(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface, VkSwapchainKHR old_swapchain);
} gSwapchain;


struct FrameData
{
    Buffer          UBO;
    VkSemaphore     present_semaphore       = {};
    VkSemaphore     render_semaphore        = {};
    VkFence         render_fence            = {};
    VkFence         compute_fence           = {};
    VkCommandBuffer graphics_command_buffer = {};
    VkCommandBuffer compute_command_buffer  = {};
    VkDescriptorSet UBO_set                 = {};
    uint32_t        image_idx               = 0;
};
std::vector<FrameData> gFrames;

VkPipeline                         gDefault_graphics_pipeline;
VkPipelineLayout                   gDefault_graphics_pipeline_layout;
std::vector<VkDescriptorSet>       gDescriptor_sets;
std::vector<VkDescriptorSetLayout> gDescriptor_set_layouts;

uint32_t WIDTH  = 800;
uint32_t HEIGHT = 600;


void Swapchain::Create(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface, VkSwapchainKHR old_swapchain)
{

    // fixme: double buffering may not be supported on some platforms
    // call to check vkGetPhysicalDeviceSurfaceCapabilitiesKHR()

    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities));

    uint32_t surface_format_count = 0;
    VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, NULL));
    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, &surface_formats[0]));

    WIDTH  = surface_capabilities.currentExtent.width;
    HEIGHT = surface_capabilities.currentExtent.height;

    if (WIDTH == 0 || HEIGHT == 0) return;

    // fixme: check with surface_capabilities.min/maxImageCount
    _image_count = 2;
    uint32_t usage_flag;

    if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        usage_flag = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    else {
        SDL_Log("Surface does not support COLOR_ATTACHMENT !");
        return;
    }

    _format = surface_formats[0].format;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface                  = surface;
    swapchain_create_info.minImageCount            = _image_count;
    swapchain_create_info.imageFormat              = surface_formats[0].format;
    swapchain_create_info.imageColorSpace          = surface_formats[0].colorSpace;
    swapchain_create_info.imageExtent              = surface_capabilities.currentExtent; // fixme: needs to be checked
    swapchain_create_info.imageArrayLayers         = 1; // we are not doing stereo rendering
    swapchain_create_info.imageUsage               = usage_flag;
    swapchain_create_info.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE; // only ever using swapchain _images with the same queue family
    swapchain_create_info.queueFamilyIndexCount    = 1;
    swapchain_create_info.pQueueFamilyIndices      = NULL;
    swapchain_create_info.preTransform             = surface_capabilities.currentTransform; // fixme: we sure?
    swapchain_create_info.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // we dont want to blend our window with other windows
    swapchain_create_info.presentMode              = present_mode;
    swapchain_create_info.clipped                  = VK_TRUE; // we don't care about pixels covered by another window
    swapchain_create_info.oldSwapchain             = old_swapchain;

    VkResult result = vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &_handle);
    VKCHECK(result);




    //
    // Query swapchain _images
    //
    if (vkGetSwapchainImagesKHR(device, _handle, &_image_count, NULL) != VK_SUCCESS) {
        SDL_Log("failed to retrieve swapchain images!");
        return;
    }
    _images.resize(_image_count);
    VKCHECK(vkGetSwapchainImagesKHR(device, _handle, &_image_count, &_images[0]));




    //
    // image views
    //
    _image_views.resize(_image_count);

    for (size_t i = 0; i < _image_count; i++) {
        VkImageViewCreateInfo view_ci = {};
        view_ci.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image                 = _images[i];
        view_ci.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format                = VK_FORMAT_B8G8R8A8_UNORM;

        view_ci.components.r                    = VK_COMPONENT_SWIZZLE_R;
        view_ci.components.g                    = VK_COMPONENT_SWIZZLE_G;
        view_ci.components.b                    = VK_COMPONENT_SWIZZLE_B;
        view_ci.components.a                    = VK_COMPONENT_SWIZZLE_A;
        view_ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.baseMipLevel   = 0;
        view_ci.subresourceRange.levelCount     = 1;
        view_ci.subresourceRange.baseArrayLayer = 0;
        view_ci.subresourceRange.layerCount     = 1;
        VKCHECK(vkCreateImageView(device, &view_ci, NULL, &_image_views[i]));
    }

    //
    // Depth image
    //

    // if (_depth_image.handle != VK_NULL_HANDLE)
    //      vkDestroyImageView(_device, _depthImageView, nullptr);
    // 	    vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);


    VkImageCreateInfo depth_image_ci {};
    depth_image_ci.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_image_ci.imageType = VK_IMAGE_TYPE_2D;
    depth_image_ci.format    = VK_FORMAT_D16_UNORM;

    depth_image_ci.extent.width  = WIDTH;
    depth_image_ci.extent.height = HEIGHT;
    depth_image_ci.extent.depth  = 1;

    depth_image_ci.mipLevels   = 1;
    depth_image_ci.arrayLayers = 1;
    depth_image_ci.samples     = VK_SAMPLE_COUNT_1_BIT;
    depth_image_ci.tiling      = VK_IMAGE_TILING_OPTIMAL; // use VK_IMAGE_TILING_LINEAR if CPU writes are intended
    depth_image_ci.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // depth_image_ci.queueFamilyIndexCount;
    // depth_image_ci.pQueueFamilyIndices;
    depth_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo vma_depth_alloc_ci {};
    vma_depth_alloc_ci.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    vma_depth_alloc_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VKCHECK(vmaCreateImage(gAllocator, &depth_image_ci, &vma_depth_alloc_ci, &_depth_image.handle, &_depth_image.vma_allocation, NULL));

    VkImageViewCreateInfo depth_view_ci {};
    depth_view_ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_ci.image    = _depth_image.handle;
    depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_ci.format   = VK_FORMAT_D16_UNORM;

    depth_view_ci.components.r                = VK_COMPONENT_SWIZZLE_R;
    depth_view_ci.components.g                = VK_COMPONENT_SWIZZLE_G;
    depth_view_ci.components.b                = VK_COMPONENT_SWIZZLE_B;
    depth_view_ci.components.a                = VK_COMPONENT_SWIZZLE_A;
    depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_ci.subresourceRange.levelCount = 1;
    depth_view_ci.subresourceRange.layerCount = 1;
    VKCHECK(vkCreateImageView(device, &depth_view_ci, NULL, &_depth_image_view));




}

bool CreateShaderModule(VkDevice device, const char *filepath, VkShaderModule *out_ShaderModule)
{

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        SDL_Log("Could not find path to %s : %s", filepath, strerror(errno));
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint32_t *buffer = (uint32_t *)malloc(fsize);
    assert(buffer);
    fread(buffer, fsize, 1, f);
    fclose(f);

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext                    = NULL;
    createInfo.codeSize                 = fsize;
    createInfo.pCode                    = buffer;

    VkShaderModule shaderModule;

    auto result = vkCreateShaderModule(device, &createInfo, NULL, &shaderModule);
    if (result != VK_SUCCESS) {
        VKCHECK(result);
        return false;
    }

    *out_ShaderModule = shaderModule;

    return true;
}



//
// Graphics Pipeline
//
static void CreateGraphicsPipeline(VkDevice device, VkPipeline *pipeline)
{
    // [x]write shaders
    // [x]compile to spv
    // [x]load spv
    // [x]create modules
    // [ ]build pipeline

    VkShaderModule vertex_shader_module;
    VkShaderModule fragment_shader_module;
    CreateShaderModule(gDevice, "shaders/standard.vert.spv", &vertex_shader_module);
    CreateShaderModule(gDevice, "shaders/standard.frag.spv", &fragment_shader_module);

    //
    // Pipeline
    //
    VkPipelineShaderStageCreateInfo shader_stage_cis[2] {};
    shader_stage_cis[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_cis[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_cis[0].module = vertex_shader_module;
    shader_stage_cis[0].pName  = "main";

    shader_stage_cis[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_cis[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_cis[1].module = fragment_shader_module;
    shader_stage_cis[1].pName  = "main";

    //
    // Raster state
    //
    VkPipelineRasterizationStateCreateInfo raster_state_ci {};
    raster_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_state_ci.depthClampEnable;
    raster_state_ci.rasterizerDiscardEnable = VK_FALSE;
    raster_state_ci.polygonMode             = VK_POLYGON_MODE_FILL;
    raster_state_ci.cullMode                = VK_CULL_MODE_BACK_BIT;
    raster_state_ci.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_state_ci.depthBiasEnable;
    raster_state_ci.depthBiasConstantFactor;
    raster_state_ci.depthBiasClamp;
    raster_state_ci.depthBiasSlopeFactor;
    raster_state_ci.lineWidth = 1.f;

    //
    // Vertex input state
    //
    const int                       BINDING_COUNT = 2;
    VkVertexInputBindingDescription vertex_binding_descriptions[BINDING_COUNT] { 0 };
    vertex_binding_descriptions[0].binding   = 0; // corresponds to vkCmdBindVertexBuffer(binding)
    vertex_binding_descriptions[0].stride    = sizeof(float) * 3;
    vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_binding_descriptions[1].binding   = 1; // corresponds to vkCmdBindVertexBuffer(binding)
    vertex_binding_descriptions[1].stride    = sizeof(float) * 3;
    vertex_binding_descriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


    const int                         ATTRIB_COUNT = 2;
    VkVertexInputAttributeDescription vertex_attrib_descriptions[ATTRIB_COUNT] { 0 };
    vertex_attrib_descriptions[0].location = 0;
    vertex_attrib_descriptions[0].binding  = 0;
    vertex_attrib_descriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrib_descriptions[0].offset   = 0; // 0: tightly packed

    vertex_attrib_descriptions[1].location = 1;
    vertex_attrib_descriptions[1].binding  = 1;
    vertex_attrib_descriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrib_descriptions[1].offset   = 0;

    VkPipelineVertexInputStateCreateInfo vertex_input_state_ci {};
    vertex_input_state_ci.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_ci.vertexBindingDescriptionCount   = ARR_COUNT(vertex_binding_descriptions);
    vertex_input_state_ci.pVertexBindingDescriptions      = vertex_binding_descriptions;
    vertex_input_state_ci.vertexAttributeDescriptionCount = ARR_COUNT(vertex_attrib_descriptions);
    vertex_input_state_ci.pVertexAttributeDescriptions    = vertex_attrib_descriptions;

    //
    // Input assembly state
    //
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_ci {};
    input_assembly_state_ci.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state_ci.primitiveRestartEnable;

    //
    // Dynamic states
    //

    VkDynamicState dynamic_states[2] {};
    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamic_state_ci {};
    dynamic_state_ci.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_ci.dynamicStateCount = ARR_COUNT(dynamic_states);
    dynamic_state_ci.pDynamicStates    = dynamic_states;

    //
    // Viewport state
    //
    VkPipelineViewportStateCreateInfo viewport_state_ci {};
    viewport_state_ci.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_ci.viewportCount = 1;
    viewport_state_ci.pViewports    = NULL;
    viewport_state_ci.scissorCount  = 1;
    viewport_state_ci.pScissors     = NULL;




    //
    // Depth/Stencil state
    //
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_ci {};
    depth_stencil_state_ci.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state_ci.depthTestEnable  = VK_TRUE; // tests wether fragment sould be discarded
    depth_stencil_state_ci.depthWriteEnable = VK_TRUE; // should depth test result be written to depth buffer?
    depth_stencil_state_ci.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL; // keeps fragments that are closer (lower depth)

    // Optional: what is stencil for[check]?
    // depth_stencil_state_ci.stencilTestEnable;
    // depth_stencil_state_ci.front;
    // depth_stencil_state_ci.back;

    // Optional: allows to specify bounds
    // depth_stencil_state_ci.depthBoundsTestEnable;
    // depth_stencil_state_ci.minDepthBounds;
    // depth_stencil_state_ci.maxDepthBounds;




    //
    // Color blend state
    //
    VkPipelineColorBlendAttachmentState color_blend_attachment_state {};
    color_blend_attachment_state.blendEnable;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
    color_blend_attachment_state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_state_ci {};
    color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_ci.logicOpEnable;
    color_blend_state_ci.logicOp           = VK_LOGIC_OP_NO_OP;
    color_blend_state_ci.attachmentCount   = 1;
    color_blend_state_ci.pAttachments      = &color_blend_attachment_state;
    color_blend_state_ci.blendConstants[0] = 1.f;
    color_blend_state_ci.blendConstants[1] = 1.f;
    color_blend_state_ci.blendConstants[2] = 1.f;
    color_blend_state_ci.blendConstants[3] = 1.f;



    //
    // Pipeline layout
    //
    VkPipelineLayoutCreateInfo layout_ci {};
    layout_ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = gDescriptor_set_layouts.size();
    layout_ci.pSetLayouts    = &gDescriptor_set_layouts[0];
    layout_ci.pushConstantRangeCount;
    layout_ci.pPushConstantRanges;

    vkCreatePipelineLayout(device, &layout_ci, NULL, &gDefault_graphics_pipeline_layout);


    //
    // Pipeline state
    //
    VkPipelineRenderingCreateInfo rendering_ci {};
    rendering_ci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_ci.colorAttachmentCount    = 1;
    rendering_ci.pColorAttachmentFormats = &gSwapchain._format;
    rendering_ci.depthAttachmentFormat   = VK_FORMAT_D16_UNORM;

    VkGraphicsPipelineCreateInfo pipeline_ci {};
    pipeline_ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.pNext               = &rendering_ci;
    pipeline_ci.stageCount          = ARR_COUNT(shader_stage_cis);
    pipeline_ci.pStages             = shader_stage_cis;
    pipeline_ci.pVertexInputState   = &vertex_input_state_ci;
    pipeline_ci.pInputAssemblyState = &input_assembly_state_ci;
    pipeline_ci.pTessellationState;
    pipeline_ci.pViewportState      = &viewport_state_ci;
    pipeline_ci.pRasterizationState = &raster_state_ci;
    pipeline_ci.pMultisampleState;
    pipeline_ci.pDepthStencilState = &depth_stencil_state_ci;
    pipeline_ci.pColorBlendState   = &color_blend_state_ci;
    pipeline_ci.pDynamicState      = &dynamic_state_ci;
    pipeline_ci.layout             = gDefault_graphics_pipeline_layout;
    pipeline_ci.renderPass;
    pipeline_ci.subpass;
    pipeline_ci.basePipelineHandle;
    pipeline_ci.basePipelineIndex;

    VKCHECK(vkCreateGraphicsPipelines(device, 0, 1, &pipeline_ci, NULL, pipeline));
}

/////////////////////////////////
// vulkan utility functions

VkResult CreateBuffer(VkBuffer *buffer, VkDeviceSize size, VmaAllocation *allocation, VkBufferUsageFlags buffer_usage, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage)
{
    VkBufferCreateInfo buffer_ci {};
    buffer_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size        = size;
    buffer_ci.usage       = buffer_usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_allocation_ci {};
    vma_allocation_ci.flags = allocation_flags;
    vma_allocation_ci.usage = memory_usage;

    return vmaCreateBuffer(gAllocator, &buffer_ci, &vma_allocation_ci, buffer, allocation, NULL);
}


#endif
