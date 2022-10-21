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
    alignas(16) glm::mat4 projection;
    alignas(16) glm::mat4 view;
};

#define MAX_RENDER_ENTITIES 1024 * 100
struct DrawDataSSBO
{
    alignas(16) glm::mat4 model;
};

struct PushConstants
{
    uint32_t draw_data_idx;
};

struct MaterialData
{
    alignas(16) glm::vec4 color;
    uint32_t base_color_texture_idx;
    uint32_t emissive_texture_idx;
    uint32_t normal_texture_idx;
    uint32_t pad;
};
