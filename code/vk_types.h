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

struct DrawDataSSBO
{
    glm::mat4 model;
    glm::mat4 joint_matrices[64];
};

struct PushConstants
{
    uint32_t draw_data_idx;
};

struct MaterialData
{
    int32_t base_color_texture_idx         = -1;
    int32_t emissive_texture_idx           = -1;
    int32_t normal_texture_idx             = -1;
    int32_t metallic_roughness_texture_idx = -1;
    float   tiling_x                       = 1.f;
    float   tiling_y                       = 1.f;
    float   metallic_factor;
    float   roughness_factor;
    alignas(16) glm::vec4 base_color_factor;
};
