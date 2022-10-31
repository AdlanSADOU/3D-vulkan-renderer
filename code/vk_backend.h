#include "backend.h"

#if defined(VULKAN) && !defined(GL)
#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

// #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLM/glm.hpp>
#include <GLM/gtx/quaternion.hpp>
#include <GLM/gtc/matrix_transform.hpp>


#if !defined(STB_IMAGE_IMPLEMENTATION)
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>
#if !defined(CGLTF_IMPLEMENTATION)
#define CGLTF_IMPLEMENTATION
#endif
#include <cgltf.h>


#define _VMA_PUBLIC_INTERFACE
#include <vk_mem_alloc.h>


// this is used for bindless ObjectData & MaterialDataSSBO shader resources
// which can be indexed for every rendered mesh
#define MAX_RENDER_ENTITIES 1024

#define SECONDS(value)                  (1000000000 * value)
#define PROMPT_GPU_SELECTION_AT_STARTUP 0
#define ARR_COUNT(arr)                  (sizeof(arr) / sizeof(arr[0]))

struct Mesh;
struct Texture;

static std::unordered_map<std::string, void *>    gCgltfData;
static std::unordered_map<std::string, Mesh *>    gSharedMesh;
static std::unordered_map<std::string, void *>    gSharedMeshes;
static std::unordered_map<std::string, Texture *> gTextures;


////////////////////////////
// Vulkan specifics
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


struct GlobalUniforms
{
    glm::mat4 projection;
    glm::mat4 view;
};




#include "vk_types.h"
#include "utils.h"
#include "camera.h"
#include "animation.h"
#include "vk_texture.h"
#include "vk_mesh.h"


Input   input          = {};
Camera *gActive_camera = NULL;

////////////// Engine Interface ///////////////////
VkResult CreateBuffer(Buffer *buffer, VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage);
void     TransitionImageLayout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, bool is_depth);
void     CreateGraphicsPipeline(VkDevice device, VkPipeline *pipeline);

////////////////////////////
// Runtime data

// VK_PRESENT_MODE_FIFO_KHR; // widest support / VSYNC
// VK_PRESENT_MODE_IMMEDIATE_KHR; // present as fast as possible, high tearing chance
// VK_PRESENT_MODE_MAILBOX_KHR; // present as fast as possible, low tearing chance
VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

SDL_Window      *gWindow = {};
VkInstance       gInstance;
VkSurfaceKHR     gSurface;
VkPhysicalDevice gPhysical_device;
VkDevice         gDevice;
VmaAllocator     gAllocator;

struct GPU
{
    VkPhysicalDevice                           _physical_device;
    VkPhysicalDeviceFeatures                   _features;
    VkPhysicalDeviceFeatures                   _features2;
    VkPhysicalDeviceLimits                     _limits;
    VkPhysicalDeviceDescriptorIndexingFeatures _descriptor_indexing_features;
} gGpu;

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
    Buffer          view_proj_uniforms;
    Buffer          draw_data_ssbo;
    Buffer          material_data_ssbo;
    VkSemaphore     present_semaphore       = {};
    VkSemaphore     render_semaphore        = {};
    VkFence         render_fence            = {};
    VkFence         compute_fence           = {};
    VkCommandBuffer graphics_command_buffer = {};
    VkCommandBuffer compute_command_buffer  = {};
    // VkDescriptorSet UBO_set                 = {};
    uint32_t image_idx = 0;
};
uint32_t               gFrame_in_flight = 0;
VkCommandBuffer        gGraphics_cmd_buffer_in_flight;
std::vector<FrameData> gFrames;
std::vector<void *>    mapped_view_proj_ptrs;
std::vector<void *>    mapped_object_data_ptrs;
std::vector<void *>    mapped_material_data_ptrs;

VkPipeline       gDefault_graphics_pipeline;
VkPipelineLayout gDefault_graphics_pipeline_layout;

VkDescriptorPool gDescriptor_pool;


std::vector<VkDescriptorSet> gView_projection_sets;
VkDescriptorSetLayout        gView_projection_set_layout;

std::vector<VkDescriptorSet> gDraw_data_sets;
VkDescriptorSetLayout        gDraw_data_set_layout;

std::vector<VkDescriptorSet> gMaterial_data_sets;
VkDescriptorSetLayout        gMaterial_data_set_layout;

const uint32_t                     MAX_TEXTURE_COUNT = 128;
uint32_t                           textures_count    = 0;
VkDescriptorSet                    gBindless_textures_set;
VkDescriptorSetLayout              gBindless_textures_set_layout;
std::vector<VkDescriptorImageInfo> gDescriptor_image_infos;
VkSampler                          gDefaultSampler;


uint32_t WIDTH  = 1180;
uint32_t HEIGHT = 720;




#endif
