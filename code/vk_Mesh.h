#pragma once
/*
*/
static uint32_t global_instance_id = 0;

struct MaterialData
{
    int32_t   base_color_texture_idx         = -1;
    int32_t   emissive_texture_idx           = -1;
    int32_t   normal_texture_idx             = -1;
    int32_t   metallic_roughness_texture_idx = -1;
    float     tiling_x                       = 1.f;
    float     tiling_y                       = 1.f;
    float     metallic_factor;
    float     roughness_factor;
    glm::vec4 base_color_factor;
};

struct cgltf_data;

struct Mesh
{
    std::vector<Joint> _joints;
    // std::vector<AnimationV2> _animations_v2;
    std::vector<Animation> _animations;
    Animation             *_current_animation     = {};
    bool                   _should_play_animation = true;


    // todo: we must eventually cache this data to avoid
    // creating GPU buffers for the same Mesh multiple times
    VkBuffer      vertex_buffer;
    VmaAllocation vertex_buffer_allocation;
    cgltf_data   *_mesh_data;

    struct SkinnedMesh
    {
        uint32_t id;

        std::vector<MaterialData> material_data {};

        // offsets to non-interleaved vertex attributes
        std::vector<VkDeviceSize> POSITION_offset;
        std::vector<VkDeviceSize> NORMAL_offset;
        std::vector<VkDeviceSize> TEXCOORD_0_offset;
        std::vector<VkDeviceSize> TEXCOORD_1_offset;
        std::vector<VkDeviceSize> JOINTS_0_offset;
        std::vector<VkDeviceSize> WEIGHTS_0_offset;

        std::vector<VkDeviceSize> index_offset;
        std::vector<uint32_t>     index_count;
    };
    std::vector<SkinnedMesh> _meshes;

    void Create(const char *path);
};

void Draw(const Mesh *model);
