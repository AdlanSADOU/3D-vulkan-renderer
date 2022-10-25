#pragma once
/*
*/
static uint32_t global_instance_id = 0;


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

    Mesh(const char *path);
    Mesh(){};
};

void Draw(const Mesh *model, VkCommandBuffer command_buffer, uint32_t frame_in_flight);
