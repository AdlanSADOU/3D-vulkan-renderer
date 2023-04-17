#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
#include <vector>

#include "common.h"

struct Swapchain
{
    VkSwapchainKHR _handle = VK_NULL_HANDLE;

    std::vector<VkImage>     _images = {};
    std::vector<VkImageView> _image_views = {};
    VkFormat                 _format;

    Image                    _msaa_image{};
    VkImageView              _msaa_image_view{};
    Image                    _depth_image = {};
    VkImageView              _depth_image_view = {};
    uint32_t                 _image_count = {};
    void                     Create(VkSurfaceKHR surface, VkSwapchainKHR old_swapchain);
};

struct FrameData
{
    Buffer          view_proj_uniforms;
    Buffer          draw_data_ssbo;
    Buffer          material_data_ssbo;
    VkSemaphore     present_semaphore = {};
    VkSemaphore     render_semaphore = {};
    VkFence         render_fence = {};
    VkFence         compute_fence = {};
    VkCommandBuffer graphics_command_buffer = {};
    VkCommandBuffer compute_command_buffer = {};
    // VkDescriptorSet UBO_set                 = {};
    uint32_t image_idx = 0;
};

struct VKDeviceContext
{
    VmaAllocator     allocator;
    VkCommandPool   graphics_command_pool;
    Swapchain swapchain;
    std::vector<FrameData> frames;

    VkDescriptorPool descriptor_pool;


    std::vector<VkDescriptorSet> view_projection_sets;
    VkDescriptorSetLayout        view_projection_set_layout;

    std::vector<VkDescriptorSet> draw_data_sets;
    VkDescriptorSetLayout        draw_data_set_layout;

    std::vector<VkDescriptorSet> material_data_sets;
    VkDescriptorSetLayout        material_data_set_layout;

    uint32_t                           texture_descriptor_counter = 0;
    const uint32_t                     MAX_TEXTURE_COUNT = 256;
    VkDescriptorSet                    bindless_textures_set;
    VkDescriptorSetLayout              bindless_textures_set_layout;
    std::vector<VkDescriptorImageInfo> descriptor_image_infos;
    VkSampler                          defaultSampler;
    VkSampler                          defaultCubeSampler;

    std::vector<void*>    mapped_view_proj_ptrs;
    std::vector<void*>    mapped_object_data_ptrs;
    std::vector<void*>    mapped_material_data_ptrs;

    VkPipeline default_graphics_pipeline;
    VkPipelineLayout default_graphics_pipeline_layout;

    bool Create();
    void AddDescriptorImageInfo(VkDescriptorImageInfo desc_image_info, bool is_cubemap);

    VkCommandBuffer BeginCommandBuffer();
    void FlushCommandBuffer(const VkCommandBuffer* command_buffer);
};