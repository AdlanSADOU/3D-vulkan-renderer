#define FRAMEWORK_EXPORTS
#include "VKTexture.h"

#include "VKDeviceContext.h"
#include "VKDevice.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL_log.h>
#include <ktxvulkan.h>

int CalculateMipLevels(int width, int height);
void GenerateMipmaps(VkCommandBuffer command_buffer, VkImage image, int mip_levels, int width, int height);

extern VKDevice gDevice;
extern VKDeviceContext gDeviceContext;

namespace FMK {

    FMK_API void Texture::Create(const char* filepath)
    {
        // note: right now we assume ONE array of textures for all textures in the game because we are using the globals
        // - descriptor_image_infos
        // - bindless_textures_set
        // - texture_descriptor_counter



        descriptor_array_idx = gDeviceContext.texture_descriptor_counter;

        int tex_width, tex_height, texChannels;

        // STBI_rgb_alpha forces alpha
        stbi_uc* pixels = stbi_load(filepath, &tex_width, &tex_height, &texChannels, STBI_rgb_alpha);
        if (!pixels)
            SDL_Log("failed to load %s", filepath);

        size_t imageSize = static_cast<size_t>(tex_width) * tex_height * 4;

        width = (uint32_t)tex_width;
        height = (uint32_t)tex_height;
        num_channels = texChannels;
        format = VK_FORMAT_R8G8B8A8_SRGB;





        Buffer staging_buffer;
        VKCHECK(CreateBuffer(&staging_buffer, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT, VMA_MEMORY_USAGE_CPU_ONLY, gDeviceContext.allocator));

        void* staging_data;
        vmaMapMemory(gDeviceContext.allocator, staging_buffer.vma_allocation, &staging_data);
        assert(pixels);
        memcpy(staging_data, pixels, imageSize);
        vmaFlushAllocation(gDeviceContext.allocator, staging_buffer.vma_allocation, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(gDeviceContext.allocator, staging_buffer.vma_allocation);
        stbi_image_free(pixels);

        int mip_levels = CalculateMipLevels(width, height);


        // Create Image
        VkImageCreateInfo ci_image = {};
        ci_image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci_image.imageType = VK_IMAGE_TYPE_2D;
        ci_image.format = format;
        ci_image.extent = { width, height, 1 };
        ci_image.mipLevels = mip_levels;
        ci_image.arrayLayers = 1;
        ci_image.samples = VK_SAMPLE_COUNT_1_BIT;
        ci_image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci_image.tiling = VK_IMAGE_TILING_OPTIMAL;
        VmaAllocationCreateInfo ci_allocation = {};
        ci_allocation.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VKCHECK(vmaCreateImage(gDeviceContext.allocator, &ci_image, &ci_allocation, &image, &allocation, NULL));




        // Staging
        auto command_buffer = gDeviceContext.BeginCommandBuffer();

        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = mip_levels;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        TransitionImageLayout(command_buffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource_range, false);

        VkBufferImageCopy buffer_image_copy{};
        buffer_image_copy.bufferOffset = 0;
        buffer_image_copy.bufferRowLength = 0;
        buffer_image_copy.bufferImageHeight = 0;
        buffer_image_copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        buffer_image_copy.imageOffset = { 0, 0, 0 }; // x, y, z
        buffer_image_copy.imageExtent = { (uint32_t)tex_width, (uint32_t)tex_height, 1 };
        vkCmdCopyBufferToImage(command_buffer, staging_buffer.handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

        GenerateMipmaps(command_buffer, image, mip_levels, width, height);

        //TransitionImageLayout(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range, false);
        gDeviceContext.FlushCommandBuffer(&command_buffer);
        vmaDestroyBuffer(gDeviceContext.allocator, staging_buffer.handle, staging_buffer.vma_allocation);

        {
            VkImageSubresourceRange subresource_range{};
            subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresource_range.baseMipLevel = 0;
            subresource_range.levelCount = mip_levels;
            subresource_range.baseArrayLayer = 0;
            subresource_range.layerCount = 1;

            VkImageViewCreateInfo ci_image_view = {};
            ci_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ci_image_view.image = image;
            ci_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ci_image_view.format = format;
            ci_image_view.components = {}; // VK_COMPONENT_SWIZZLE_IDENTITY = 0
            ci_image_view.subresourceRange = subresource_range;
            VKCHECK(vkCreateImageView(gDevice.device, &ci_image_view, NULL, &view));
        }




        VkDescriptorImageInfo desc_image_info{};

        // notes: we can pass a sampler as an argument eventually when the need arises
        // but for now it's fine
        desc_image_info.sampler = gDeviceContext.defaultSampler;
        desc_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image_info.imageView = view;

        //gDeviceContext.descriptor_image_infos[texture_descriptor_counter] = desc_image_info;
        gDeviceContext.AddDescriptorImageInfo(desc_image_info, false);

        VkWriteDescriptorSet setWrites[2]{};
        setWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        setWrites[1].dstSet = gDeviceContext.bindless_textures_set;
        setWrites[1].dstBinding = 1;
        setWrites[1].dstArrayElement = 0;
        setWrites[1].descriptorCount = (uint32_t)gDeviceContext.descriptor_image_infos.size();
        setWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        setWrites[1].pImageInfo = gDeviceContext.descriptor_image_infos.data();
        setWrites[1].pBufferInfo = 0;
        vkUpdateDescriptorSets(gDevice.device, 1, &setWrites[1], 0, NULL);

    }


    void Texture::CreateCubemapKTX(const char* filepath, VkFormat format)
    {
        ktxResult result;
        ktxTexture* ktx_texture;

        result = ktxTexture_CreateFromNamedFile(filepath, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
        assert(result == KTX_SUCCESS);

        //format = ktxTexture_GetVkFormat(ktx_texture);
        //this->format = format;
        this->format = VK_FORMAT_R8G8B8A8_UNORM;
        width = ktx_texture->baseWidth;
        height = ktx_texture->baseHeight;
        mip_levels = ktx_texture->numLevels;
        ktx_uint8_t* ktx_texture_data = ktxTexture_GetData(ktx_texture);
        ktx_size_t ktx_texture_data_size = ktxTexture_GetDataSize(ktx_texture);


        VkFormat ktxformat = ktxTexture_GetVkFormat(ktx_texture);



        Buffer staging_buffer;
        VKCHECK(CreateBuffer(&staging_buffer, ktx_texture_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT, VMA_MEMORY_USAGE_CPU_ONLY, gDeviceContext.allocator));
        void* staging_data;
        vmaMapMemory(gDeviceContext.allocator, staging_buffer.vma_allocation, &staging_data);
        assert(ktx_texture_data);
        memcpy(staging_data, ktx_texture_data, ktx_texture_data_size);
        vmaFlushAllocation(gDeviceContext.allocator, staging_buffer.vma_allocation, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(gDeviceContext.allocator, staging_buffer.vma_allocation);



        // Create optimal tiled target image
        VkImageCreateInfo ci_image{};
        ci_image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci_image.imageType = VK_IMAGE_TYPE_2D;
        ci_image.format = this->format;
        ci_image.mipLevels = mip_levels;
        ci_image.samples = VK_SAMPLE_COUNT_1_BIT;
        ci_image.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci_image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci_image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ci_image.extent = { width, height, 1 };
        ci_image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        // Cube faces count as array layers in Vulkan
        ci_image.arrayLayers = 6;
        // This flag is required for cube map images
        ci_image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VmaAllocationCreateInfo ci_allocation = {};
        ci_allocation.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VKCHECK(vmaCreateImage(gDeviceContext.allocator, &ci_image, &ci_allocation, &image, &allocation, NULL));


        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel = 0;
        subresource_range.baseArrayLayer = 0;
        subresource_range.levelCount = mip_levels;
        subresource_range.layerCount = 6;


        VkImageViewCreateInfo ci_image_view = {};
        ci_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci_image_view.image = image;
        ci_image_view.format = this->format;
        ci_image_view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        ci_image_view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        ci_image_view.subresourceRange = subresource_range;

        VKCHECK(vkCreateImageView(gDevice.device, &ci_image_view, NULL, &view));






        // begin staging command buffer
        auto command_buffer = gDeviceContext.BeginCommandBuffer();

        TransitionImageLayout(command_buffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource_range, false);

        // Setup buffer copy regions for each face including all of its miplevels
        std::vector<VkBufferImageCopy> image_copy_regions;
        uint32_t offset = 0;

        for (uint32_t face = 0; face < 6; face++)
        {
            for (uint32_t level = 0; level < mip_levels; level++)
            {
                // Calculate offset into staging buffer for the current mip level and face
                ktx_size_t offset;
                KTX_error_code ret = ktxTexture_GetImageOffset(ktx_texture, level, 0, face, &offset);
                assert(ret == KTX_SUCCESS);
                VkBufferImageCopy image_copy_region = {};
                image_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_copy_region.imageSubresource.mipLevel = level;
                image_copy_region.imageSubresource.baseArrayLayer = face;
                image_copy_region.imageSubresource.layerCount = 1;
                image_copy_region.imageExtent.width = ktx_texture->baseWidth >> level;
                image_copy_region.imageExtent.height = ktx_texture->baseHeight >> level;
                image_copy_region.imageExtent.depth = 1;
                image_copy_region.bufferOffset = offset;
                image_copy_regions.push_back(image_copy_region);
            }
        }

        vkCmdCopyBufferToImage(command_buffer, staging_buffer.handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(image_copy_regions.size()), image_copy_regions.data());

        TransitionImageLayout(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range, false);
        gDeviceContext.FlushCommandBuffer(&command_buffer);

        vmaDestroyBuffer(gDeviceContext.allocator, staging_buffer.handle, staging_buffer.vma_allocation);



        {
            // cube sampler
            VkSamplerCreateInfo ci_sampler = {};
            ci_sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            ci_sampler.minFilter = VK_FILTER_LINEAR;
            ci_sampler.magFilter = VK_FILTER_LINEAR;
            ci_sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            ci_sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            ci_sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            ci_sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            ci_sampler.maxLod = (float)mip_levels;
            ci_sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            //If the samplerAnisotropy feature is not enabled, anisotropyEnable must be VK_FALSE
            // todo: investigate anisotropy feature
            ci_sampler.anisotropyEnable = VK_TRUE;
            ci_sampler.maxAnisotropy = 8;
            VKCHECK(vkCreateSampler(gDevice.device, &ci_sampler, NULL, &gDeviceContext.defaultCubeSampler));
        }

        VkDescriptorImageInfo desc_image_info{};

        // notes: we can pass a sampler as an argument eventually when the need arises
        // but for now it's fine
        desc_image_info.sampler = gDeviceContext.defaultCubeSampler;
        desc_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image_info.imageView = view;
        gDeviceContext.AddDescriptorImageInfo(desc_image_info, true);


        VkWriteDescriptorSet setWrites[2]{};
        setWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        setWrites[0].dstSet = gDeviceContext.bindless_textures_set;
        setWrites[0].dstBinding = 0;
        setWrites[0].dstArrayElement = 0;
        setWrites[0].descriptorCount = 1;
        setWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        setWrites[0].pImageInfo = &desc_image_info;
        setWrites[0].pBufferInfo = 0;

        //setWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        //setWrites[1].dstSet = bindless_textures_set;
        //setWrites[1].dstBinding = 1;
        //setWrites[1].dstArrayElement = 0;
        //setWrites[1].descriptorCount = (uint32_t)descriptor_image_infos.size();
        //setWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        //setWrites[1].pImageInfo = descriptor_image_infos.data();
        //setWrites[1].pBufferInfo = 0;
        vkUpdateDescriptorSets(gDevice.device, 1, &setWrites[0], 0, NULL);


        //texture_descriptor_counter++;

    }

    void Texture::Destroy()
    {
        vkDestroyImageView(gDevice.device, view, NULL);
        vmaDestroyImage(gDeviceContext.allocator, image, allocation);

        //todo:
        // remove corresponding entry from VKDeviceContext::descriptor_image_infos
    }

}

static int CalculateMipLevels(int width, int height)
{
    int max_dimension = glm::max(width, height);
    int levels = static_cast<int>(glm::floor(glm::log2((float)max_dimension)));

    return levels;
}

static void GenerateMipmaps(VkCommandBuffer command_buffer, VkImage image, int mip_levels, int width, int height)
{
    VkImageSubresourceRange subresource_range{};
    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount = 1;
    subresource_range.levelCount = 1;

    int32_t mipWidth = width;
    int32_t mipHeight = height;

    for (int32_t i = 1; i < mip_levels; i++) {
        subresource_range.baseMipLevel = i - 1;

        TransitionImageLayout(command_buffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            subresource_range, false);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(command_buffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        TransitionImageLayout(command_buffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresource_range, false);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    subresource_range.baseMipLevel = mip_levels - 1;
    TransitionImageLayout(command_buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        subresource_range, false);
}