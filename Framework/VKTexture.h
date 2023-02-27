#pragma once
#include "common.h"


namespace FMK {
    struct Texture
    {
        const char* name;
        VkImage image;
        VmaAllocation allocation;
        VkImageView view;
        VkFormat format;

        uint32_t mip_levels;
        uint32_t width;
        uint32_t height;
        uint32_t num_channels;

        // Shaders have an array of all textures (bindless textures)
        // this is the index into that array
        // see gDescriptor_image_infos & gBindless_textures_set
        uint32_t descriptor_array_idx;

        FMK_API void Create(const char* filepath);
        FMK_API void CreateCubemapKTX(const char* filepath, VkFormat format);
        FMK_API void Destroy();
    };
}