

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define _VMA_PUBLIC_INTERFACE
#define CGLTF_IMPLEMENTATION
#include "vk_renderer.h"
#include "common.h"

#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <ktxvulkan.h>


#pragma warning(disable: 6011)

// node(ad) this is used for bindless ObjectData & MaterialDataSSBO shader resources
// which can be indexed for every rendered mesh
constexpr int MAX_RENDER_ENTITIES = 1024 * 10;
constexpr int PROMPT_GPU_SELECTION_AT_STARTUP = 0;
VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_8_BIT;

#define SECONDS(value)                  (1000000000 * value)
#define ARR_COUNT(arr)                  (sizeof(arr) / sizeof(arr[0]))

namespace Renderer {


    // this is where resources are cached right now
    std::unordered_map<std::string, void*>    gCgltfData;
    std::unordered_map<std::string, Mesh*>    gSharedMesh;
    std::unordered_map<std::string, void*>    gSharedMeshes;
    std::unordered_map<std::string, Texture*> gTextures;

    static Camera* gActive_camera = NULL;

    ////////////// Engine Interface ///////////////////
    VkCommandBuffer BeginCommandBuffer();
    void FlushCommandBuffer(const VkCommandBuffer* command_buffer);
    VkResult CreateBuffer(Buffer* buffer, VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage);
    void     TransitionImageLayout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource_range, bool is_depth);
    void     CreateGraphicsPipeline(VkDevice device, VkPipeline* pipeline);

    ////////////////////////////
    // Runtime data

    // VK_PRESENT_MODE_FIFO_KHR; // widest support / VSYNC
    // VK_PRESENT_MODE_IMMEDIATE_KHR; // present as fast as possible, high tearing chance
    // VK_PRESENT_MODE_MAILBOX_KHR; // present as fast as possible, low tearing chance
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    static SDL_Window* gWindow;
    static VkInstance       gInstance;
    static VkSurfaceKHR     gSurface;
    static VkPhysicalDevice gPhysical_device;
    static VkDevice         gDevice;
    static VmaAllocator     gAllocator;


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
        void                     Create(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface, VkSwapchainKHR old_swapchain);
    } gSwapchain;



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
    uint32_t               gFrame_in_flight = 0;
    VkCommandBuffer        gGraphics_cmd_buffer_in_flight;
    std::vector<FrameData> gFrames;
    std::vector<void*>    mapped_view_proj_ptrs;
    std::vector<void*>    mapped_object_data_ptrs;
    std::vector<void*>    mapped_material_data_ptrs;

    VkPipeline       gDefault_graphics_pipeline;
    VkPipelineLayout gDefault_graphics_pipeline_layout;

    VkDescriptorPool gDescriptor_pool;


    std::vector<VkDescriptorSet> gView_projection_sets;
    VkDescriptorSetLayout        gView_projection_set_layout;

    std::vector<VkDescriptorSet> gDraw_data_sets;
    VkDescriptorSetLayout        gDraw_data_set_layout;

    std::vector<VkDescriptorSet> gMaterial_data_sets;
    VkDescriptorSetLayout        gMaterial_data_set_layout;

    const uint32_t                     MAX_TEXTURE_COUNT = 256;
    VkDescriptorSet                    gBindless_textures_set;
    VkDescriptorSetLayout              gBindless_textures_set_layout;
    std::vector<VkDescriptorImageInfo> gDescriptor_image_infos;
    VkSampler                          gDefaultSampler;
    VkSampler                          gDefaultCubeSampler;

    static uint32_t WIDTH = 1180;
    static uint32_t HEIGHT = 720;


    //
    // Camera
    //
    void Camera::CameraCreate(glm::vec3 position, float fov, float aspect, float near, float far)
    {
        _forward = { 0, 0, -1 };
        _right = {};
        _up = { 0, 1, 0 };
        _yaw = 0;
        _pitch = 0;
        _fov = fov;
        _aspect = 16 / (float)9;
        _near = near;
        _far = far;
        _position = position;

        _at = _position + _forward;
        _projection = glm::perspective(glm::radians(_fov), _aspect, _near, _far);
        _view = glm::lookAt(_position, _at, _up);

        mode = FREEFLY;
    }

    void Camera::CameraUpdate(Input* input, float dt, glm::vec3 target)
    {
        switch (mode) {
        case FREEFLY:
        {
            if (input->up) _position += _forward * _speed * dt;
            if (input->down) _position -= _forward * _speed * dt;
            if (input->left) _position -= glm::normalize(glm::cross(_forward, _up)) * _speed * dt;
            if (input->right) _position += glm::normalize(glm::cross(_forward, _up)) * _speed * dt;

            float factor = 0;
            if (input->E) factor -= 1.f;
            if (input->Q) factor += 1.f;
            _position.y += factor * dt / _sensitivity;

            static float xrelPrev = 0;
            static float yrelPrev = 0;
            int          xrel;
            int          yrel;

            SDL_GetRelativeMouseState(&xrel, &yrel);
            if (input->mouse.right) {
                SDL_SetRelativeMouseMode(SDL_TRUE);

                if (xrelPrev != xrel) _yaw += (float)xrel * _sensitivity;
                if (yrelPrev != yrel) _pitch -= (float)yrel * _sensitivity;
            }
            else
                SDL_SetRelativeMouseMode(SDL_FALSE);

            _forward.x = cosf(glm::radians(_yaw)) * cosf(glm::radians(_pitch));
            _forward.y = sinf(glm::radians(_pitch));
            _forward.z = sinf(glm::radians(_yaw)) * cosf(glm::radians(_pitch));
            _forward = glm::normalize(_forward);
            xrelPrev = (float)xrel;
            yrelPrev = (float)yrel;

            _at = _position + _forward;
            _view = glm::lookAt(_position, _at, _up);
        }
        break;

        case THIRD_PERSON:
        {

            float factor = 0;
            if (input->E) factor -= 1.f;
            if (input->Q) factor += 1.f;
            _position.y += factor * dt / _sensitivity;

            static float xrelPrev = 0;
            static float yrelPrev = 0;
            int          xrel;
            int          yrel;

            SDL_GetRelativeMouseState(&xrel, &yrel);
            if (input->mouse.right) {
                SDL_SetRelativeMouseMode(SDL_TRUE);

                if (xrelPrev != xrel) _yaw += (float)xrel * _sensitivity;
                if (yrelPrev != yrel) _pitch -= (float)yrel * _sensitivity;
            }
            else
                SDL_SetRelativeMouseMode(SDL_FALSE);



            static glm::vec3 offset{};
            offset.x = cosf(glm::radians(_yaw)) * cosf(glm::radians(_pitch)) * 60;
            offset.y = sinf(glm::radians(_pitch)) * 60 * -1;
            offset.z = sinf(glm::radians(_yaw)) * cosf(glm::radians(_pitch)) * 60;
            _forward = (offset);
            _right = (glm::cross(_forward, _up));

            xrelPrev = (float)xrel;
            yrelPrev = (float)yrel;

            _position = target + offset;
            _view = glm::lookAt(_position, target + glm::vec3(0, 20, 0), _up);
            //_view = glm::mat4(glm::mat3(_view));
        }
        break;

        default:
            break;
        }

        _projection = glm::perspective(_fov, _aspect, _near, _far);
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


    //
    // Texture
    //
    void LoadAndCacheTexture(int32_t* texture_idx, const char* texture_uri, const char* folder_path)
    {
        std::string full_path;
        full_path.append(folder_path);
        full_path.append(texture_uri);

        Texture* t = (Texture*)malloc(sizeof(Texture));
        assert(t != NULL);

        t->Create(full_path.c_str());
        t->name = _strdup(texture_uri);

        gTextures.insert({ std::string(texture_uri), t });

        *texture_idx = t->descriptor_array_idx;
    }


    uint32_t gTexture_descriptor_counter = 0;
    void Texture::Create(const char* filepath)
    {
        // note: right now we assume ONE array of textures for all textures in the game because we are using the globals
        // - gDescriptor_image_infos
        // - gBindless_textures_set
        // - gTexture_descriptor_counter




        descriptor_array_idx = gTexture_descriptor_counter;

        int tex_width, tex_height, texChannels;

        // STBI_rgb_alpha forces alpha
        stbi_uc* pixels = stbi_load(filepath, &tex_width, &tex_height, &texChannels, STBI_rgb_alpha);
        if (!pixels)
            SDL_Log("failed to load %s", filepath);

        size_t imageSize = static_cast<size_t>(tex_width) * tex_height * 4;

        width = (uint32_t)tex_width;
        height = (uint32_t)tex_height;
        num_channels = texChannels;
        format = VK_FORMAT_R8G8B8A8_UNORM;





        Buffer staging_buffer;
        VKCHECK(CreateBuffer(&staging_buffer, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT, VMA_MEMORY_USAGE_CPU_ONLY));

        void* staging_data;
        vmaMapMemory(gAllocator, staging_buffer.vma_allocation, &staging_data);
        assert(pixels);
        memcpy(staging_data, pixels, imageSize);
        vmaFlushAllocation(gAllocator, staging_buffer.vma_allocation, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(gAllocator, staging_buffer.vma_allocation);
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

        VmaAllocationCreateInfo ci_allocation = {};
        ci_allocation.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VKCHECK(vmaCreateImage(gAllocator, &ci_image, &ci_allocation, &image, &allocation, NULL));




        // Staging
        auto command_buffer = BeginCommandBuffer();

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
        FlushCommandBuffer(&command_buffer);
        vmaDestroyBuffer(gAllocator, staging_buffer.handle, staging_buffer.vma_allocation);

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
            VKCHECK(vkCreateImageView(gDevice, &ci_image_view, NULL, &view));
        }




        VkDescriptorImageInfo desc_image_info{};

        // notes: we can pass a sampler as an argument eventually when the need arises
        // but for now it's fine
        desc_image_info.sampler = gDefaultSampler;
        desc_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image_info.imageView = view;

        gDescriptor_image_infos[gTexture_descriptor_counter] = desc_image_info;

        VkWriteDescriptorSet setWrites[2]{};
        setWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        setWrites[1].dstSet = gBindless_textures_set;
        setWrites[1].dstBinding = 1;
        setWrites[1].dstArrayElement = 0;
        setWrites[1].descriptorCount = (uint32_t)gDescriptor_image_infos.size();
        setWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        setWrites[1].pImageInfo = gDescriptor_image_infos.data();
        setWrites[1].pBufferInfo = 0;
        vkUpdateDescriptorSets(gDevice, 1, &setWrites[1], 0, NULL);


        gTexture_descriptor_counter++;
    }


    void Texture::CreateCubemapKTX(const char* filepath, VkFormat format)
    {
        ktxResult result;
        ktxTexture* ktx_texture;

        result = ktxTexture_CreateFromNamedFile(filepath, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
        assert(result == KTX_SUCCESS);

        format = ktxTexture_GetVkFormat(ktx_texture);
        this->format = format;
        width = ktx_texture->baseWidth;
        height = ktx_texture->baseHeight;
        mip_levels = ktx_texture->numLevels;
        ktx_uint8_t* ktx_texture_data = ktxTexture_GetData(ktx_texture);
        ktx_size_t ktx_texture_data_size = ktxTexture_GetDataSize(ktx_texture);


        VkFormat ktxformat = ktxTexture_GetVkFormat(ktx_texture);
        printf("ktxTexture_GetVkFormat: %d, format: %d", ktxformat, format);



        Buffer staging_buffer;
        VKCHECK(CreateBuffer(&staging_buffer, ktx_texture_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT, VMA_MEMORY_USAGE_CPU_ONLY));
        void* staging_data;
        vmaMapMemory(gAllocator, staging_buffer.vma_allocation, &staging_data);
        assert(ktx_texture_data);
        memcpy(staging_data, ktx_texture_data, ktx_texture_data_size);
        vmaFlushAllocation(gAllocator, staging_buffer.vma_allocation, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(gAllocator, staging_buffer.vma_allocation);



        // Create optimal tiled target image
        VkImageCreateInfo ci_image{};
        ci_image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci_image.imageType = VK_IMAGE_TYPE_2D;
        ci_image.format = format;
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
        VKCHECK(vmaCreateImage(gAllocator, &ci_image, &ci_allocation, &image, &allocation, NULL));


        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel = 0;
        subresource_range.baseArrayLayer = 0;
        subresource_range.levelCount = mip_levels;
        subresource_range.layerCount = 6;


        VkImageViewCreateInfo ci_image_view = {};
        ci_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci_image_view.image = image;
        ci_image_view.format = format;
        ci_image_view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        ci_image_view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        ci_image_view.subresourceRange = subresource_range;

        VKCHECK(vkCreateImageView(gDevice, &ci_image_view, NULL, &view));






        // begin staging command buffer
        auto command_buffer = BeginCommandBuffer();

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
        FlushCommandBuffer(&command_buffer);

        vmaDestroyBuffer(gAllocator, staging_buffer.handle, staging_buffer.vma_allocation);



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
            VKCHECK(vkCreateSampler(gDevice, &ci_sampler, NULL, &gDefaultCubeSampler));
        }

        VkDescriptorImageInfo desc_image_info{};

        // notes: we can pass a sampler as an argument eventually when the need arises
        // but for now it's fine
        desc_image_info.sampler = gDefaultCubeSampler;
        desc_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image_info.imageView = view;

        //gDescriptor_image_infos[gTexture_descriptor_counter] = desc_image_info;

        VkWriteDescriptorSet setWrites[2]{};
        setWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        setWrites[0].dstSet = gBindless_textures_set;
        setWrites[0].dstBinding = 0;
        setWrites[0].dstArrayElement = 0;
        setWrites[0].descriptorCount = 1;
        setWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        setWrites[0].pImageInfo = &desc_image_info;
        setWrites[0].pBufferInfo = 0;

        //setWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        //setWrites[1].dstSet = gBindless_textures_set;
        //setWrites[1].dstBinding = 1;
        //setWrites[1].dstArrayElement = 0;
        //setWrites[1].descriptorCount = (uint32_t)gDescriptor_image_infos.size();
        //setWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        //setWrites[1].pImageInfo = gDescriptor_image_infos.data();
        //setWrites[1].pBufferInfo = 0;
        vkUpdateDescriptorSets(gDevice, 1, &setWrites[0], 0, NULL);


        //gTexture_descriptor_counter++;

    }

    void Texture::Destroy()
    {
        vkDestroyImageView(gDevice, view, NULL);
        vmaDestroyImage(gAllocator, image, allocation);
    }











    //
    // Mesh
    //




    float AnimationGetClipDuration(cgltf_animation* animationClip)
    {
        float duration = 0;
        float lastMax = 0;

        for (size_t i = 0; i < animationClip->samplers_count; i++) {
            cgltf_accessor* input = animationClip->samplers[i].input;

            for (size_t j = 0; j < ARR_COUNT(input->max); j++) {
                if (input->has_max && input->max[j] > lastMax)
                    lastMax = input->max[j];
            }
            duration = lastMax;
        }
        return duration;
    }

    int AnimationMaxSampleCount(cgltf_animation* animationClip)
    {
        int last_max = 0;

        for (size_t i = 0; i < animationClip->samplers_count; i++) {
            auto* input = animationClip->samplers[i].input;

            if (input->count > last_max) {
                last_max = (int)input->count;
            }
        }
        return last_max;
    }

    float readFloatFromAccessor(cgltf_accessor* accessor, cgltf_size idx)
    {
        float value;
        // SDL_Log("idx:[%d]", idx);
        if (cgltf_accessor_read_float(accessor, idx, &value, accessor->count))
            return value;
        else
            return 0;
    }

    glm::vec3 getVec3AtKeyframe(cgltf_animation_sampler* sampler, int keyframe)
    {
        // todo: check for failure?
        void* values = (void*)((uint8_t*)sampler->output->buffer_view->buffer->data + sampler->output->buffer_view->offset);
        return ((glm::vec3*)values)[keyframe];
    }
    glm::quat getQuatAtKeyframe(cgltf_animation_sampler* sampler, int keyframe)
    {
        // todo: check for failure?
        void* values = (void*)((uint8_t*)sampler->output->buffer_view->buffer->data + sampler->output->buffer_view->offset);
        return ((glm::quat*)values)[keyframe];
    }

    /////////////////////////////////////

    void AnimationUpdate(Animation* anim, float dt)
    {

        /*if (!model->_should_play_animation) return;

        if (!model->_current_animation) {
            SDL_Log("CurrentAnimation not set");
            return;
        }

        auto anim = model->_current_animation;*/

        assert(anim->handle);
        assert(anim->duration > 0);
        anim->isPlaying = true;

        anim->globalTimer += dt;
        float animTime = fmodf(anim->globalTimer, anim->duration);


        static int iterationNb = 0;

        if (iterationNb++ == 0) {
            SDL_Log("playing:[%s] | duration: %fsec", ((cgltf_animation*)anim->handle)->name, anim->duration);
        }

        // For each Joint
        int channel_idx = 0;
        for (size_t joint_idx = 0; joint_idx < anim->_joints.size(); joint_idx++) {

            auto channels = ((cgltf_animation*)anim->handle)->channels;
            int  currentKey = -1;
            int  nextKey = -1;

            auto* sampler = channels[channel_idx].sampler;
            for (int timestamp_idx = 0; timestamp_idx < sampler->input->count - 1; timestamp_idx++) {
                float sampled_time = readFloatFromAccessor(sampler->input, static_cast<cgltf_size>(timestamp_idx) + 1);
                //float sampled_time_prev;

                if (sampled_time > animTime) {
                    currentKey = timestamp_idx;
                    nextKey = currentKey + 1;
                    break;
                }
            }


            Transform currentPoseTransform;

            if (channels[channel_idx].target_path == cgltf_animation_path_type_translation) {
                currentPoseTransform.translation = getVec3AtKeyframe(channels[channel_idx].sampler, currentKey);
                ++channel_idx;
            }
            if (channels[channel_idx].target_path == cgltf_animation_path_type_rotation) {
                currentPoseTransform.rotation = getQuatAtKeyframe(channels[channel_idx].sampler, currentKey);
                ++channel_idx;
            }
            if (channels[channel_idx].target_path == cgltf_animation_path_type_scale) {
                currentPoseTransform.scale = getVec3AtKeyframe(channels[channel_idx].sampler, currentKey);
                ++channel_idx;
            }

            auto current_pose_matrix = currentPoseTransform.GetLocalMatrix();
            auto parent_idx = anim->_joints[joint_idx].parent;

            // comppute global matrix for current joint
            if (parent_idx == -1 && joint_idx > 0)
                anim->_joints[joint_idx].global_joint_matrix = current_pose_matrix;
            else if (joint_idx == 0)
                anim->_joints[0].global_joint_matrix = current_pose_matrix;
            else
                anim->_joints[joint_idx].global_joint_matrix = anim->_joints[parent_idx].global_joint_matrix * current_pose_matrix;

            anim->joint_matrices[joint_idx] = anim->_joints[joint_idx].global_joint_matrix * anim->_joints[joint_idx].inv_bind_matrix;
        } // End of Joints loop
    }











    //
    // Utils
    //
    bool LoadCgltfData(const char* path, cgltf_data** data)
    {
        cgltf_options options = {};
        cgltf_result  result = cgltf_parse_file(&options, path, &(*data));

        if (cgltf_parse_file(&options, path, &(*data)) == cgltf_result_success) {
            if (cgltf_load_buffers(&options, (*data), path) == cgltf_result_success) {
                if ((result = cgltf_validate((*data))) != cgltf_result_success) {
                    SDL_Log("cgltf validation error");
                    return false;
                }
                result = cgltf_load_buffers(&options, (*data), path);
            }
        }
        else {
            SDL_Log("Failed to load [%s]", path);
            return false;
        }

        return true;
    }

    // Callee allocates dst
    static errno_t GetPathFolder(char** dst, const char* src, uint32_t srcLength)
    {
        assert(src);
        assert(srcLength > 0);

        size_t size = std::string(src).find_last_of('/');
        *dst = (char*)malloc(sizeof(char) * size + 1);
        return strncpy_s(*(char**)dst, size + 2, src, size + 1);
    }




    // not to be messed with!
    // it is automatically incremented and assigned as a "unique" id
    // to each submesh of a unique mesh that is loaded (we cache meshes)
    // so that the corresponding material data can be retieves in shaders
    static uint32_t global_instance_id = 0;

    void Mesh::Create(const char* mesh_filepath)
    {
        if (gCgltfData.contains(mesh_filepath)) {
            _mesh_data = (cgltf_data*)gCgltfData[mesh_filepath];

        }
        else {
            cgltf_data* data;
            LoadCgltfData(mesh_filepath, &data);

            gCgltfData.insert({ std::string(mesh_filepath), (void*)data });
            _mesh_data = data;
        }

        if (gSharedMesh.contains(mesh_filepath)) {
            SDL_Log("Mesh is already cached : %s", mesh_filepath);
            return;
        }


        size_t vertex_buffer_size = _mesh_data->buffers->size;

        VkBuffer staging_buffer;
        VmaAllocation staging_buffer_allocation;
        {
            VkBufferCreateInfo buffer_ci{};
            buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size = vertex_buffer_size;
            buffer_ci.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo vma_allocation_ci{};
            vma_allocation_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            vma_allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            // TODO: upload to GPU exclusive memory instead of a mapped ptr!!!

            vmaCreateBuffer(gAllocator, &buffer_ci, &vma_allocation_ci, &staging_buffer, &staging_buffer_allocation, NULL);
            void* mapped_vertex_buffer_ptr;
            vmaMapMemory(gAllocator, staging_buffer_allocation, &mapped_vertex_buffer_ptr);

            // for now, as a quick first pass, we load and keep the entire gltf .bin data
            // in shared GPU memory, bad bad
            // future improvements should upload geometry data to GPU exclusive vertex buffer (verts & indices in the same buffer)
            // might call it geometry_buffer for lack of a better name.
            // I haven't done the math to see if we should pre-cache ALL geometry data into a single buffer
            memcpy(mapped_vertex_buffer_ptr, _mesh_data->buffers->data, vertex_buffer_size);
            vmaUnmapMemory(gAllocator, staging_buffer_allocation);
        }



        // create & allocate vertex buffer
        {

            VkBufferCreateInfo buffer_ci{};
            buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size = vertex_buffer_size;
            buffer_ci.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo vma_allocation_ci{};
            vma_allocation_ci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            vma_allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            // TODO: upload to GPU exclusive memory instead of a mapped ptr!!!

            vmaCreateBuffer(gAllocator, &buffer_ci, &vma_allocation_ci, &vertex_buffer, &vertex_buffer_allocation, NULL);
        }


        VkBufferCopy2 copy_regions{};
        copy_regions.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        copy_regions.pNext = NULL;
        copy_regions.srcOffset = 0;
        copy_regions.dstOffset = 0;
        copy_regions.size = vertex_buffer_size;

        VkCopyBufferInfo2 copy_buffer_info{};
        copy_buffer_info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        copy_buffer_info.pNext = NULL;
        copy_buffer_info.srcBuffer = staging_buffer;
        copy_buffer_info.dstBuffer = vertex_buffer;
        copy_buffer_info.regionCount = 1;
        copy_buffer_info.pRegions = &copy_regions;



        VkCommandBufferBeginInfo cmd_buffer_begin_info = {};
        cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VKCHECK(vkBeginCommandBuffer(gFrames[0].graphics_command_buffer, &cmd_buffer_begin_info));
        vkCmdCopyBuffer2(gFrames[0].graphics_command_buffer, &copy_buffer_info);
        VKCHECK(vkEndCommandBuffer(gFrames[0].graphics_command_buffer));

        // Submit command buffer and copy data from staging buffer to a vertex buffer
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &gFrames[0].graphics_command_buffer;
        VKCHECK(vkQueueSubmit(gQueues._graphics, 1, &submit_info, VK_NULL_HANDLE));
        VKCHECK(vkDeviceWaitIdle(gDevice));








        _meshes.resize(_mesh_data->meshes_count);

        for (size_t mesh_idx = 0; mesh_idx < _mesh_data->meshes_count; mesh_idx++) {
            auto mesh = &_meshes[mesh_idx];
            auto primitives_count = _mesh_data->meshes[mesh_idx].primitives_count;

            mesh->material_data.resize(primitives_count);
            mesh->POSITION_offset.resize(primitives_count);
            mesh->NORMAL_offset.resize(primitives_count);
            mesh->TEXCOORD_0_offset.resize(primitives_count);
            mesh->TEXCOORD_1_offset.resize(primitives_count);
            mesh->JOINTS_0_offset.resize(primitives_count);
            mesh->WEIGHTS_0_offset.resize(primitives_count);
            mesh->index_offset.resize(primitives_count);
            mesh->index_count.resize(primitives_count);


            for (size_t submesh_idx = 0; submesh_idx < primitives_count; submesh_idx++) {
                //
                // Vertex Attributes
                //
                cgltf_primitive* primitive = &_mesh_data->meshes[mesh_idx].primitives[submesh_idx];

                for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                    cgltf_attribute* attrib = &primitive->attributes[attrib_idx];
                    cgltf_buffer_view* view = attrib->data->buffer_view;

                    if (attrib->type == cgltf_attribute_type_position)
                        mesh->POSITION_offset[submesh_idx] = view->offset;
                    else if (attrib->type == cgltf_attribute_type_normal)
                        mesh->NORMAL_offset[submesh_idx] = view->offset;
                    else if (attrib->type == cgltf_attribute_type_texcoord && strcmp(attrib->name, "TEXCOORD_0") == 0)
                        mesh->TEXCOORD_0_offset[submesh_idx] = view->offset;
                    else if (attrib->type == cgltf_attribute_type_texcoord && strcmp(attrib->name, "TEXCOORD_1") == 0)
                        mesh->TEXCOORD_1_offset[submesh_idx] = view->offset;
                    else if (attrib->type == cgltf_attribute_type_joints)
                        mesh->JOINTS_0_offset[submesh_idx] = view->offset;
                    else if (attrib->type == cgltf_attribute_type_weights)
                        mesh->WEIGHTS_0_offset[submesh_idx] = view->offset;
                }

                mesh->index_offset[submesh_idx] = primitive->indices->buffer_view->offset;
                mesh->index_count[submesh_idx] = (uint32_t)primitive->indices->count;


                //
                // Material data
                //
                auto material_data = &mesh->material_data[submesh_idx];

                bool material_data_is_null = !primitive->material;
                if (material_data_is_null) continue;

                bool no_mettalic_worklow = !primitive->material->has_pbr_metallic_roughness;
                if (no_mettalic_worklow) {
                    SDL_Log("Only PBR Metal Roughness is supported: %s", _mesh_data->meshes[mesh_idx].primitives->material->name);
                    continue;
                }


                auto base_color_texture = primitive->material->pbr_metallic_roughness.base_color_texture.texture;
                auto metallic_roughness_texture = primitive->material->pbr_metallic_roughness.metallic_roughness_texture.texture;
                auto normal_texture = primitive->material->normal_texture.texture;
                auto emissive_texture = primitive->material->emissive_texture.texture;

                char* textures_folder_path;
                GetPathFolder(&textures_folder_path, mesh_filepath, (uint32_t)strlen(mesh_filepath));

                // fixme cache texture
                if (base_color_texture) {
                    LoadAndCacheTexture(&material_data->base_color_texture_idx, base_color_texture->image->uri, textures_folder_path);
                }
                if (metallic_roughness_texture) {
                    LoadAndCacheTexture(&material_data->metallic_roughness_texture_idx, metallic_roughness_texture->image->uri, textures_folder_path);
                }
                if (normal_texture) {
                    LoadAndCacheTexture(&material_data->metallic_roughness_texture_idx, normal_texture->image->uri, textures_folder_path);
                }
                if (emissive_texture) {
                    LoadAndCacheTexture(&material_data->metallic_roughness_texture_idx, emissive_texture->image->uri, textures_folder_path);
                }

                material_data->base_color_factor.r = primitive->material->pbr_metallic_roughness.base_color_factor[0];
                material_data->base_color_factor.g = primitive->material->pbr_metallic_roughness.base_color_factor[1];
                material_data->base_color_factor.b = primitive->material->pbr_metallic_roughness.base_color_factor[2];
                material_data->base_color_factor.a = primitive->material->pbr_metallic_roughness.base_color_factor[3];

                material_data->metallic_factor = primitive->material->pbr_metallic_roughness.metallic_factor;
                material_data->roughness_factor = primitive->material->pbr_metallic_roughness.roughness_factor;
            }

            mesh->instance_id = global_instance_id++;
        }

        //fixme cache mesh
        //gSharedMesh.insert({ "assets/warrior/warrior.gltf", this });


        //
        // Animations
        //
        auto data = _mesh_data; // yayaya
        // if no animations to handle, we just end here
        if (!data->animations_count) return;

        _animations.resize(data->animations_count);
        for (size_t animation_idx = 0; animation_idx < data->animations_count; animation_idx++) {
            Animation* anim = &_animations[animation_idx];

            /*_animations[animation_idx]; = {
                .handle = &data->animations[animation_idx],
                .name = data->animations[animation_idx].name,
                .duration = AnimationGetClipDuration(&data->animations[animation_idx]),
            };*/
            // fixme: delete

            anim->handle = &data->animations[animation_idx];
            anim->name = data->animations[animation_idx].name;
            anim->duration = AnimationGetClipDuration(&data->animations[animation_idx]);


            anim->joint_matrices.resize(data->skins->joints_count);

            for (size_t i = 0; i < anim->joint_matrices.size(); i++) {
                anim->joint_matrices[i] = glm::mat4(1);
            }
        }




        ////// Setup Skeleton: flatten **joints array
        assert(data->skins_count == 1);

        auto skin = data->skins;
        auto joints = skin->joints;
        auto rig_node = skin->joints[0]->parent;
        _joints.resize(skin->joints_count);

        for (size_t joint_idx = 0; joint_idx < skin->joints_count; joint_idx++) {
            _joints[joint_idx].name = joints[joint_idx]->name; // fixme: animation.joints[i].name will crash if cgltf_data is freed;

            // set inv_bind_matrix
            uint8_t* data_buffer = (uint8_t*)skin->inverse_bind_matrices->buffer_view->buffer->data;
            auto       offset = skin->inverse_bind_matrices->buffer_view->offset;
            glm::mat4* inv_bind_matrices = ((glm::mat4*)(data_buffer + offset));

            _joints[joint_idx].inv_bind_matrix = inv_bind_matrices[joint_idx];

            auto current_joint_parent = joints[joint_idx]->parent;

            if (joint_idx == 0 || current_joint_parent == rig_node)
                _joints[joint_idx].parent = -1;
            else {

                for (int8_t i = 1; i < skin->joints_count; i++) {
                    if (joints[i]->parent == current_joint_parent) {
                        _joints[joint_idx].parent = i - 1;
                        break;
                    }
                }
            }
        }

        for (size_t i = 0; i < _animations.size(); i++) {
            _animations[i]._joints = _joints;
        }





        // ////////// todo: figure out if we should actually go this route
        // ////// Process animation samples
        // auto anims_data = data->animations;

        // _animations_v2.resize(data->animations_count);
        // for (size_t anim_idx = 0; anim_idx < data->animations_count; anim_idx++) {
        //     int sample_count = AnimationMaxSampleCount(anims_data);
        //     _animations_v2[anim_idx].samples.resize(skin->joints_count);

        //     // For each Joint
        //     for (size_t joint_idx = 0; joint_idx < skin->joints_count; joint_idx++) {

        //         // For each Channel
        //         for (size_t chan_idx = 0; chan_idx < anims_data->channels_count; chan_idx++) {
        //             cgltf_animation_channel *channel = &anims_data->channels[chan_idx];

        //             // if channel target matches current joint
        //             if (channel->target_node == skin->joints[joint_idx]) {

        //                 cgltf_animation_sampler *sampler = channel->sampler;

        //                 _animations_v2[anim_idx].samples[joint_idx].target_joint = joint_idx;

        //                 auto   input_data_ptr    = (uint8_t *)sampler->input->buffer_view->buffer->data;
        //                 auto   input_data_offset = sampler->input->buffer_view->offset;
        //                 float *timestamps_data   = (float *)(input_data_ptr + input_data_offset);

        //                 auto   output_data_ptr     = (uint8_t *)sampler->output->buffer_view->buffer->data;
        //                 auto   output_data_offset  = sampler->output->buffer_view->offset;
        //                 float *transformation_data = (float *)(output_data_ptr + output_data_offset);

        //                 if (channel->target_path == cgltf_animation_path_type_translation) {
        //                     _animations_v2[anim_idx].samples[joint_idx].translation_channel.timestamps   = timestamps_data;
        //                     _animations_v2[anim_idx].samples[joint_idx].translation_channel.translations = (glm::vec3 *)transformation_data;

        //                 } else if (channel->target_path == cgltf_animation_path_type_rotation) {
        //                     _animations_v2[anim_idx].samples[joint_idx].rotation_channel.timestamps = timestamps_data;
        //                     _animations_v2[anim_idx].samples[joint_idx].rotation_channel.rotations  = (glm::quat *)transformation_data;
        //                 } else if (channel->target_path == cgltf_animation_path_type_scale) {
        //                     _animations_v2[anim_idx].samples[joint_idx].scale_channel.timestamps = timestamps_data;
        //                     _animations_v2[anim_idx].samples[joint_idx].scale_channel.scales     = (glm::vec3 *)transformation_data; // fixme: we might want to scope this to uniform scale only
        //                 }
        //             }
        //         } // End of channels loop for the current joint
        //     } // End of Joints loop
        // }
    }

    void Draw(const Mesh* model)
    {
        VkBuffer buffers[6]{};

        // if (_animations.size() > 0)
        {
            buffers[0] = model->vertex_buffer;
            buffers[1] = model->vertex_buffer;
            buffers[2] = model->vertex_buffer;
            buffers[3] = model->vertex_buffer;
            buffers[4] = model->vertex_buffer;
            buffers[5] = model->vertex_buffer;
        }

        for (size_t mesh_idx = 0; mesh_idx < model->_meshes.size(); mesh_idx++) {

            for (size_t submesh_idx = 0; submesh_idx < model->_meshes[mesh_idx].index_count.size(); submesh_idx++) {

                VkDeviceSize offsets[6]{};
                /*POSITION  */ offsets[0] = model->_meshes[mesh_idx].POSITION_offset[submesh_idx];
                /*NORMAL    */ offsets[1] = model->_meshes[mesh_idx].NORMAL_offset[submesh_idx];
                /*TEXCOORD_0*/ offsets[2] = model->_meshes[mesh_idx].TEXCOORD_0_offset[submesh_idx];
                /*TEXCOORD_1*/ offsets[3] = model->_meshes[mesh_idx].TEXCOORD_1_offset[submesh_idx];
                /*JOINTS_0  */ offsets[4] = model->_meshes[mesh_idx].JOINTS_0_offset[submesh_idx];
                /*WEIGHTS_0 */ offsets[5] = model->_meshes[mesh_idx].WEIGHTS_0_offset[submesh_idx];


                // fixme provide SetMaterialData() ?
                ((MaterialData*)mapped_material_data_ptrs[gFrame_in_flight])[model->_meshes[mesh_idx].instance_id] = model->_meshes[mesh_idx].material_data[submesh_idx];

                vkCmdBindVertexBuffers(gGraphics_cmd_buffer_in_flight, 0, ARR_COUNT(buffers), buffers, offsets);
                vkCmdBindIndexBuffer(gGraphics_cmd_buffer_in_flight, model->vertex_buffer, model->_meshes[mesh_idx].index_offset[submesh_idx], VK_INDEX_TYPE_UINT16);
                vkCmdDrawIndexed(gGraphics_cmd_buffer_in_flight, model->_meshes[mesh_idx].index_count[submesh_idx], 1, 0, 0, model->_meshes[mesh_idx].instance_id);

            }
        }
    }



    //Shader uses descriptor slot 3.0 (expected `VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`)
    //but not declared in pipeline layout The Vulkan spec states : layout must be consistent with all shaders specified in pStages














    //void DrawIndexed(const VkBuffer* vertex_buffers, uint32_t buffer_count, const VkDeviceSize* vertex_offsets, const VkBuffer index_buffer, uint32_t index_offset, uint32_t index_count, const MaterialData* material_data, uint32_t mesh_id)
    //{
    //	((MaterialData*)mapped_material_data_ptrs[gFrame_in_flight])[mesh_id] = *material_data;
    //
    //	vkCmdBindVertexBuffers(gGraphics_cmd_buffer_in_flight, 0, buffer_count, vertex_buffers, vertex_offsets);
    //	vkCmdBindIndexBuffer(gGraphics_cmd_buffer_in_flight, index_buffer, index_offset, VK_INDEX_TYPE_UINT16);
    //	vkCmdDrawIndexed(gGraphics_cmd_buffer_in_flight, index_count, 1, 0, 0, mesh_id);
    //}






    void BeginRendering()
    {

        // gActive_camera->CameraUpdate(&input, dt, {0, 0, 40});
        // gActive_camera->_aspect = WIDTH / (float)HEIGHT;


        VkResult result;

        gGraphics_cmd_buffer_in_flight = gFrames[gFrame_in_flight].graphics_command_buffer;

        {
            VKCHECK(vkWaitForFences(gDevice, 1, &gFrames[gFrame_in_flight].render_fence, VK_TRUE, SECONDS(1)));
            VKCHECK(vkResetFences(gDevice, 1, &gFrames[gFrame_in_flight].render_fence));
            VKCHECK(result = vkAcquireNextImageKHR(gDevice, gSwapchain._handle, SECONDS(1), 0, gFrames[gFrame_in_flight].render_fence, &gFrames[gFrame_in_flight].image_idx));
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                gSwapchain.Create(gDevice, gPhysical_device, gSurface, gSwapchain._handle);
            }
        }

        VKCHECK(vkResetCommandBuffer(gGraphics_cmd_buffer_in_flight, 0));

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKCHECK(vkBeginCommandBuffer(gGraphics_cmd_buffer_in_flight, &begin_info));





        //
        // Swapchain image layout transition for Rendering
        //
        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.levelCount = 1;
        subresource_range.layerCount = 1;

        TransitionImageLayout(gGraphics_cmd_buffer_in_flight, gSwapchain._images[gFrames[gFrame_in_flight].image_idx],
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, subresource_range, false);

        TransitionImageLayout(gGraphics_cmd_buffer_in_flight, gSwapchain._msaa_image.handle,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, subresource_range, false);


        VkClearValue clear_values[2] = {};
        clear_values[0].color = { .13f, .13f, .13f, 1.f };

        VkRenderingAttachmentInfo color_attachment_info[2] = {};

        color_attachment_info[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment_info[0].imageView = gSwapchain._msaa_image_view;
        color_attachment_info[0].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        color_attachment_info[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_info[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_info[0].clearValue = clear_values[0];

        color_attachment_info[0].resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        color_attachment_info[0].resolveImageView = gSwapchain._image_views[gFrames[gFrame_in_flight].image_idx];
        color_attachment_info[0].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;

        /*color_attachment_info[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment_info[1].imageView = gSwapchain._msaa_image_view;
        color_attachment_info[1].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        color_attachment_info[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_info[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_info[1].clearValue = clear_values[0];
*/

        clear_values[1].depthStencil.depth = { 1.f };

        VkRenderingAttachmentInfo depth_attachment_info = {};
        depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment_info.imageView = gSwapchain._depth_image_view;
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment_info.clearValue = clear_values[1];

        VkRenderingInfo rendering_info = {};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.extent = { WIDTH, HEIGHT };
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment_info[0];
        rendering_info.pDepthAttachment = &depth_attachment_info;
        rendering_info.pStencilAttachment; // todo
        vkCmdBeginRendering(gGraphics_cmd_buffer_in_flight, &rendering_info);





        ////////////////////////////////////////////
        // Rendering
        //

        vkCmdBindPipeline(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline);

        vkCmdBindDescriptorSets(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 0, 1, &gView_projection_sets[gFrame_in_flight], 0, NULL);
        vkCmdBindDescriptorSets(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 1, 1, &gDraw_data_sets[gFrame_in_flight], 0, NULL);
        vkCmdBindDescriptorSets(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 2, 1, &gMaterial_data_sets[gFrame_in_flight], 0, NULL);
        vkCmdBindDescriptorSets(gGraphics_cmd_buffer_in_flight, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 3, 1, &gBindless_textures_set, 0, NULL);

        ((GlobalUniforms*)mapped_view_proj_ptrs[gFrame_in_flight])->projection = gActive_camera->_projection;
        ((GlobalUniforms*)mapped_view_proj_ptrs[gFrame_in_flight])->view = gActive_camera->_view;
        ((GlobalUniforms*)mapped_view_proj_ptrs[gFrame_in_flight])->projection[1][1] *= -1;

        VkViewport viewport{};
        viewport.minDepth = 0;
        viewport.maxDepth = 1;
        viewport.width = (float)WIDTH;
        viewport.height = (float)HEIGHT;

        VkRect2D scissor{};
        scissor.extent.width = WIDTH;
        scissor.extent.height = HEIGHT;

        vkCmdSetViewport(gGraphics_cmd_buffer_in_flight, 0, 1, &viewport);
        vkCmdSetScissor(gGraphics_cmd_buffer_in_flight, 0, 1, &scissor);
    }


    //////////////////////////////////////////////
    void EndRendering() {
        vkCmdEndRendering(gGraphics_cmd_buffer_in_flight);

        //
        // Swapchain image layout transition for presenting
        //
        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.levelCount = 1;
        subresource_range.layerCount = 1;

        TransitionImageLayout(gGraphics_cmd_buffer_in_flight, gSwapchain._images[gFrames[gFrame_in_flight].image_idx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresource_range, false);
        TransitionImageLayout(gGraphics_cmd_buffer_in_flight, gSwapchain._msaa_image.handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresource_range, false);

        VKCHECK(vkEndCommandBuffer(gGraphics_cmd_buffer_in_flight));



        //
        // Command Buffer submission
        //
        {
            VkCommandBufferSubmitInfo command_buffer_submit_info = {};
            command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_info.pNext = NULL;
            command_buffer_submit_info.commandBuffer = gGraphics_cmd_buffer_in_flight;
            command_buffer_submit_info.deviceMask;

            VkSemaphoreSubmitInfo wait_semephore_info = {};
            wait_semephore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            wait_semephore_info.semaphore = gFrames[gFrame_in_flight].present_semaphore;
            wait_semephore_info.value;
            wait_semephore_info.stageMask = VK_PIPELINE_STAGE_NONE;
            wait_semephore_info.deviceIndex;

            VkSemaphoreSubmitInfo signal_semephore_info = {};
            signal_semephore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signal_semephore_info.semaphore = gFrames[gFrame_in_flight].render_semaphore;
            signal_semephore_info.value;
            signal_semephore_info.stageMask = VK_PIPELINE_STAGE_NONE;
            signal_semephore_info.deviceIndex;

            VkSubmitInfo2 submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submit_info.flags;
            submit_info.waitSemaphoreInfoCount = 0;
            // submit_info.pWaitSemaphoreInfos      = &wait_semephore_info; // wait for image to be presented before rendering again
            submit_info.commandBufferInfoCount = 1;
            submit_info.pCommandBufferInfos = &command_buffer_submit_info;
            submit_info.signalSemaphoreInfoCount = 1;
            submit_info.pSignalSemaphoreInfos = &signal_semephore_info;

            VKCHECK(vkWaitForFences(gDevice, 1, &gFrames[gFrame_in_flight].render_fence, VK_TRUE, SECONDS(1)));
            VKCHECK(vkResetFences(gDevice, 1, &gFrames[gFrame_in_flight].render_fence));

            VKCHECK(vkQueueSubmit2(gQueues._graphics, 1, &submit_info, gFrames[gFrame_in_flight].render_fence));
        }


        {
            //
            // Presenting
            //
            VkPresentInfoKHR present_info = {};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &gFrames[gFrame_in_flight].render_semaphore; // the semaphore to wait upon before presenting
            present_info.swapchainCount = 1;
            present_info.pSwapchains = &gSwapchain._handle;
            present_info.pImageIndices = &gFrames[gFrame_in_flight].image_idx;
            present_info.pResults;

            VkResult result;
            VKCHECK(result = vkQueuePresentKHR(gQueues._present, &present_info));
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                gSwapchain.Create(gDevice, gPhysical_device, gSurface, gSwapchain._handle);
            }
        }

        ++gFrame_in_flight;
        gFrame_in_flight = gFrame_in_flight % gSwapchain._image_count;
    }


    bool InitRenderer()
    {
        SDL_Init(SDL_INIT_EVERYTHING);
        auto w_flags = (SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        gWindow = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, w_flags);





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
            const char** required_instance_extensions = new const char* [required_extensions_count];
            SDL_Vulkan_GetInstanceExtensions(gWindow, &required_extensions_count, required_instance_extensions);


            uint32_t layers_count;
            vkEnumerateInstanceLayerProperties(&layers_count, NULL);
            std::vector<VkLayerProperties> layer_properties(layers_count);
            vkEnumerateInstanceLayerProperties(&layers_count, &layer_properties[0]);

            // validation layers
            const char* validation_layers[] = {
                { "VK_LAYER_KHRONOS_validation" },
                { "VK_LAYER_KHRONOS_synchronization2" },
                // { "VK_LAYER_LUNARG_monitor" },
            };


            VkApplicationInfo application_info = {};
            application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application_info.pNext = NULL;
            application_info.pApplicationName = "name";
            application_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
            application_info.pEngineName = "engine name";
            application_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
            application_info.apiVersion = VK_API_VERSION_1_3;

            VkInstanceCreateInfo create_info_instance = {};
#if _DEBUG
            VkValidationFeatureEnableEXT enabled_validation_feature[] = {
                VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
                // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            };

            VkValidationFeaturesEXT validation_features = {};
            validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validation_features.enabledValidationFeatureCount = ARR_COUNT(enabled_validation_feature);
            validation_features.pEnabledValidationFeatures = enabled_validation_feature;

            create_info_instance.pNext = &validation_features;
            create_info_instance.enabledLayerCount = ARR_COUNT(validation_layers);
            create_info_instance.ppEnabledLayerNames = validation_layers;
#endif

            create_info_instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info_instance.flags = 0;
            create_info_instance.pApplicationInfo = &application_info;
            create_info_instance.enabledExtensionCount = required_extensions_count;
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

            struct tmpGpu
            {
                VkPhysicalDeviceType type;
                const char* type_name;
                const char* name;
                uint32_t             index;
            };
            std::vector<tmpGpu> available_gpus(physical_device_count);

            uint32_t selected_physical_device_idx = -1;
            for (uint32_t i = 0; i < physical_device_count; i++) {
                auto device_type = physical_device_properties[i].deviceType;
                auto name = physical_device_properties[i].deviceName;

                if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                    available_gpus[i].type_name = "DISCRETE_GPU";
                    available_gpus[i].index = i;
                }
                if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                    available_gpus[i].type_name = "INTEGRATED_GPU";
                    available_gpus[i].index = i;
                }
                if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU) {
                    available_gpus[i].type_name = "CPU (APU)";
                    available_gpus[i].index = i;
                }
                if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
                    available_gpus[i].type_name = "VIRTUAL_GPU";
                    available_gpus[i].index = i;
                }
                if (device_type == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER) {

                    available_gpus[i].type_name = "UNKNOWN";
                    available_gpus[i].index = i;
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
                }
                else if (available_gpus[i].type = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                    selected_physical_device_idx = available_gpus[i].index;
            }

#endif
            gPhysical_device = physical_devices[selected_physical_device_idx];
            SDL_Log("Selected GPU:[%s][%s]", available_gpus[selected_physical_device_idx].type_name, available_gpus[selected_physical_device_idx].name);

            if (physical_device_properties[selected_physical_device_idx].apiVersion < VK_API_VERSION_1_3) {
                SDL_Log("GPU does not support Vulkan 1.3 Profile, consider updating drivers");
            }


            gGpu._physical_device = physical_devices[selected_physical_device_idx];
            gGpu._limits = physical_device_properties[selected_physical_device_idx].limits;

            gGpu._descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

            VkPhysicalDeviceFeatures2 physical_device_features2{};
            physical_device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physical_device_features2.pNext = &gGpu._descriptor_indexing_features;

            vkGetPhysicalDeviceFeatures2(gGpu._physical_device, &physical_device_features2);
            vkGetPhysicalDeviceFeatures(gGpu._physical_device, &gGpu._features);

            gGpu._features2 = physical_device_features2.features;
        }





        //
        // Queues
        //
        {
            uint32_t queue_family_properties_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(gPhysical_device, &queue_family_properties_count, NULL);
            VkQueueFamilyProperties* queue_family_properties = new VkQueueFamilyProperties[queue_family_properties_count];
            vkGetPhysicalDeviceQueueFamilyProperties(gPhysical_device, &queue_family_properties_count, queue_family_properties);

            if (queue_family_properties == NULL) {
                // some error message
                return 0;
            }

            VkBool32* queue_idx_supports_present = new VkBool32[queue_family_properties_count];
            for (uint32_t i = 0; i < queue_family_properties_count; i++) {
                vkGetPhysicalDeviceSurfaceSupportKHR(gPhysical_device, i, gSurface, &queue_idx_supports_present[i]);
            }

            uint32_t graphics_queue_family_idx = UINT32_MAX;
            uint32_t compute_queue_family_idx = UINT32_MAX;
            uint32_t present_queue_family_idx = UINT32_MAX;

            for (uint32_t i = 0; i < queue_family_properties_count; i++) {
                if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    if (graphics_queue_family_idx == UINT32_MAX)
                        graphics_queue_family_idx = i;

                if (queue_idx_supports_present[i] == VK_TRUE) {
                    graphics_queue_family_idx = i;
                    present_queue_family_idx = i;
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
            gQueues._compute_queue_family_idx = compute_queue_family_idx;
            gQueues._present_queue_family_idx = present_queue_family_idx;
        }





        ///
        /// Device
        ///
        {

            // todo(ad): not used right now
            uint32_t device_properties_count = 0;
            VKCHECK(vkEnumerateDeviceExtensionProperties(gPhysical_device, NULL, &device_properties_count, NULL));
            VkExtensionProperties* device_extension_properties = new VkExtensionProperties[device_properties_count];
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

            std::vector<VkDeviceQueueCreateInfo> create_info_device_queues{};

            VkDeviceQueueCreateInfo ci_graphics_queue{};
            ci_graphics_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            ci_graphics_queue.pNext = NULL;
            ci_graphics_queue.flags = 0;
            ci_graphics_queue.queueFamilyIndex = gQueues._graphics_queue_family_idx;
            ci_graphics_queue.queueCount = 1;
            ci_graphics_queue.pQueuePriorities = queue_priorities;
            create_info_device_queues.push_back(ci_graphics_queue);

            VkDeviceQueueCreateInfo ci_compute_queue{};
            ci_compute_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            ci_compute_queue.pNext = NULL;
            ci_compute_queue.flags = 0;
            ci_compute_queue.queueFamilyIndex = gQueues._compute_queue_family_idx;
            ci_compute_queue.queueCount = 1;
            ci_compute_queue.pQueuePriorities = queue_priorities;
            create_info_device_queues.push_back(ci_compute_queue);


            VkPhysicalDeviceFeatures enabled_physical_device_features{};
            enabled_physical_device_features.samplerAnisotropy = VK_TRUE;


            // VkPhysicalDeviceVulkan11Features gpu_vulkan_11_features {};
            // gpu_vulkan_11_features.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            // gpu_vulkan_11_features.shaderDrawParameters = VK_TRUE;

            // if (gGpu._descriptor_indexing_features.runtimeDescriptorArray && gGpu._descriptor_indexing_features.descriptorBindingPartiallyBound)


            VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
            descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            descriptor_indexing_features.runtimeDescriptorArray = VK_TRUE;
            descriptor_indexing_features.descriptorBindingPartiallyBound = VK_TRUE;

            VkPhysicalDeviceSynchronization2Features synchronization2_features{};
            synchronization2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
            synchronization2_features.pNext = &descriptor_indexing_features;
            synchronization2_features.synchronization2 = VK_TRUE;

            VkPhysicalDeviceRobustness2FeaturesEXT robustness_feature_ext{};
            robustness_feature_ext.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
            robustness_feature_ext.pNext = &synchronization2_features;
            robustness_feature_ext.nullDescriptor = VK_TRUE;

            VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{};
            dynamic_rendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
            dynamic_rendering.pNext = &robustness_feature_ext;
            dynamic_rendering.dynamicRendering = VK_TRUE;

            // gpu_vulkan_11_features.pNext = &robustness_feature_ext;
            const char* enabled_device_extension_names[] = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
                // VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, // core in 1.3
                // VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // core in 1.3
                // VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
            };

            VkDeviceCreateInfo create_info_device = {};
            create_info_device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            create_info_device.pNext = &dynamic_rendering; // seens to be required even if it's core in 1.3
            create_info_device.flags = 0;
            create_info_device.queueCreateInfoCount = 1;
            create_info_device.pQueueCreateInfos = &create_info_device_queues[0];
            create_info_device.enabledExtensionCount = ARR_COUNT(enabled_device_extension_names);
            create_info_device.ppEnabledExtensionNames = enabled_device_extension_names;
            create_info_device.pEnabledFeatures = &enabled_physical_device_features;

            VKCHECK(vkCreateDevice(gPhysical_device, &create_info_device, NULL, &gDevice));

            if (gQueues._seperate_present_queue) {
                vkGetDeviceQueue(gDevice, gQueues._present_queue_family_idx, 0, &gQueues._present);
            }
            else {
                vkGetDeviceQueue(gDevice, gQueues._graphics_queue_family_idx, 0, &gQueues._graphics);
                gQueues._present = gQueues._graphics;
            }
            vkGetDeviceQueue(gDevice, gQueues._compute_queue_family_idx, 0, &gQueues._compute);
        }




        //
        // VMA allocator
        //
        {
            VmaAllocatorCreateInfo vma_allocator_ci{};
            vma_allocator_ci.instance = gInstance;
            vma_allocator_ci.device = gDevice;
            vma_allocator_ci.physicalDevice = gPhysical_device;

            vmaCreateAllocator(&vma_allocator_ci, &gAllocator);
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
            command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            command_pool_ci.queueFamilyIndex = gQueues._graphics_queue_family_idx;
            VKCHECK(vkCreateCommandPool(gDevice, &command_pool_ci, NULL, &gGraphics_command_pool));



            //
            // Per frame objects
            //
            for (size_t i = 0; i < gSwapchain._image_count; i++) {
                VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
                command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                command_buffer_allocate_info.commandPool = gGraphics_command_pool;
                command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                command_buffer_allocate_info.commandBufferCount = 1;
                VKCHECK(vkAllocateCommandBuffers(gDevice, &command_buffer_allocate_info, &gFrames[i].graphics_command_buffer));

                VkFenceCreateInfo fence_ci = {};
                fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                VKCHECK(vkCreateFence(gDevice, &fence_ci, NULL, &gFrames[i].render_fence));

                VkSemaphoreCreateInfo semaphoreCreateInfo = {};
                semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                semaphoreCreateInfo.pNext = NULL;
                semaphoreCreateInfo.flags = 0;

                VKCHECK(vkCreateSemaphore(gDevice, &semaphoreCreateInfo, NULL, &gFrames[i].present_semaphore));
                VKCHECK(vkCreateSemaphore(gDevice, &semaphoreCreateInfo, NULL, &gFrames[i].render_semaphore));
            }
        }



        {
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VKCHECK(vkBeginCommandBuffer(gFrames[0].graphics_command_buffer, &begin_info));

            VkImageSubresourceRange subresource_range{};
            subresource_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            subresource_range.levelCount = 1;
            subresource_range.layerCount = 1;
            TransitionImageLayout(gFrames[0].graphics_command_buffer, gSwapchain._depth_image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, subresource_range, true);

            VKCHECK(vkEndCommandBuffer(gFrames[0].graphics_command_buffer));


            VkCommandBufferSubmitInfo command_buffer_submit_info = {};
            command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_info.pNext = NULL;
            command_buffer_submit_info.commandBuffer = gFrames[0].graphics_command_buffer;

            VkSubmitInfo2 submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submit_info.flags;
            submit_info.commandBufferInfoCount = 1;
            submit_info.pCommandBufferInfos = &command_buffer_submit_info;

            VKCHECK(vkQueueSubmit2(gQueues._graphics, 1, &submit_info, NULL));
            vkDeviceWaitIdle(gDevice);
        }


        //
        // Descriptor pool
        //
        {
            // we are creating a pool that can potentially hold 16 descriptor sets that hold swapchain.image_count number of uniform buffer descriptors
            VkDescriptorPoolSize descriptor_pool_sizes[3]{};
            descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_pool_sizes[0].descriptorCount = gSwapchain._image_count * 2; // total amount of a given descriptor type accross all sets

            descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptor_pool_sizes[1].descriptorCount = gSwapchain._image_count * 4; // total amount of a given descriptor type accross all sets

            descriptor_pool_sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_pool_sizes[2].descriptorCount = MAX_TEXTURE_COUNT; // total amount of a given descriptor type accross all sets

            VkDescriptorPoolCreateInfo descriptor_pool_ci{};
            descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            descriptor_pool_ci.maxSets = gSwapchain._image_count * 4;
            descriptor_pool_ci.poolSizeCount = ARR_COUNT(descriptor_pool_sizes); // 1 PoolSize = TYPE count of descriptors
            descriptor_pool_ci.pPoolSizes = descriptor_pool_sizes;
            VKCHECK(vkCreateDescriptorPool(gDevice, &descriptor_pool_ci, NULL, &gDescriptor_pool));


            //
            // Descriptor set layouts
            //

            // set 0
            // the layout for the set at binding 0 that will reference a MVP uniform blok
            VkDescriptorSetLayoutBinding view_projection_set_layout_binding{};
            view_projection_set_layout_binding.binding = 0;
            view_projection_set_layout_binding.descriptorCount = 1; // 1 instance of MVP block for this binding
            view_projection_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            view_projection_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            gView_projection_sets.resize(gSwapchain._image_count);


            // set 1
            VkDescriptorSetLayoutBinding draw_data_set_layout_binding{};
            draw_data_set_layout_binding.binding = 0;
            draw_data_set_layout_binding.descriptorCount = 1; //
            draw_data_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            draw_data_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            gDraw_data_sets.resize(gSwapchain._image_count);

            // set 2
            VkDescriptorSetLayoutBinding material_data_set_layout_binding{};
            material_data_set_layout_binding.binding = 0;
            material_data_set_layout_binding.descriptorCount = 1; //
            material_data_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            material_data_set_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            gMaterial_data_sets.resize(gSwapchain._image_count);


            {
                VkDescriptorSetLayoutCreateInfo set_layouts_ci{};
                set_layouts_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                set_layouts_ci.pNext = NULL;
                set_layouts_ci.flags = 0;
                set_layouts_ci.bindingCount = 1;
                set_layouts_ci.pBindings = &view_projection_set_layout_binding;

                // we need <frame_count> descriptor sets that each reference 1 MVP uniform block for each frame in flight
                VKCHECK(vkCreateDescriptorSetLayout(gDevice, &set_layouts_ci, NULL, &gView_projection_set_layout));
            }

            {
                VkDescriptorSetLayoutCreateInfo set_layouts_ci{};
                set_layouts_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                set_layouts_ci.pNext = NULL;
                set_layouts_ci.flags = 0;
                set_layouts_ci.bindingCount = 1;
                set_layouts_ci.pBindings = &draw_data_set_layout_binding;

                // we need <frame_count> descriptor sets that each reference 1 MVP uniform block for each frame in flight
                VKCHECK(vkCreateDescriptorSetLayout(gDevice, &set_layouts_ci, NULL, &gDraw_data_set_layout));
            }

            {
                VkDescriptorSetLayoutCreateInfo set_layouts_ci{};
                set_layouts_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                set_layouts_ci.pNext = NULL;
                set_layouts_ci.flags = 0;
                set_layouts_ci.bindingCount = 1;
                set_layouts_ci.pBindings = &material_data_set_layout_binding;

                // we need <frame_count> descriptor sets that each reference 1 MVP uniform block for each frame in flight
                VKCHECK(vkCreateDescriptorSetLayout(gDevice, &set_layouts_ci, NULL, &gMaterial_data_set_layout));
            }


            for (size_t i = 0; i < gSwapchain._image_count; i++) {
                {
                    VkDescriptorSetAllocateInfo descriptor_set_alloc_info{};
                    descriptor_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    descriptor_set_alloc_info.descriptorPool = gDescriptor_pool;
                    descriptor_set_alloc_info.descriptorSetCount = 1;
                    descriptor_set_alloc_info.pSetLayouts = &gView_projection_set_layout;
                    VKCHECK(vkAllocateDescriptorSets(gDevice, &descriptor_set_alloc_info, &gView_projection_sets[i]));
                }

                {
                    VkDescriptorSetAllocateInfo descriptor_set_alloc_info{};
                    descriptor_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    descriptor_set_alloc_info.descriptorPool = gDescriptor_pool;
                    descriptor_set_alloc_info.descriptorSetCount = 1;
                    descriptor_set_alloc_info.pSetLayouts = &gDraw_data_set_layout;
                    VKCHECK(vkAllocateDescriptorSets(gDevice, &descriptor_set_alloc_info, &gDraw_data_sets[i]));
                }

                {
                    VkDescriptorSetAllocateInfo descriptor_set_alloc_info{};
                    descriptor_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    descriptor_set_alloc_info.descriptorPool = gDescriptor_pool;
                    descriptor_set_alloc_info.descriptorSetCount = 1;
                    descriptor_set_alloc_info.pSetLayouts = &gMaterial_data_set_layout;
                    VKCHECK(vkAllocateDescriptorSets(gDevice, &descriptor_set_alloc_info, &gMaterial_data_sets[i]));
                }
            }
        }





        //
        // Shader uniforms & Descriptor set updates/writes
        //
        {
            mapped_view_proj_ptrs.resize(gSwapchain._image_count);
            mapped_object_data_ptrs.resize(gSwapchain._image_count);
            mapped_material_data_ptrs.resize(gSwapchain._image_count);

            for (size_t i = 0; i < gSwapchain._image_count; i++) {

                {
                    CreateBuffer(&gFrames[i].view_proj_uniforms, sizeof(GlobalUniforms),
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

                    vmaMapMemory(gAllocator, gFrames[i].view_proj_uniforms.vma_allocation, &mapped_view_proj_ptrs[i]);

                    CreateBuffer(&gFrames[i].draw_data_ssbo, sizeof(ObjectData) * MAX_RENDER_ENTITIES,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

                    vmaMapMemory(gAllocator, gFrames[i].draw_data_ssbo.vma_allocation, &mapped_object_data_ptrs[i]);

                    CreateBuffer(&gFrames[i].material_data_ssbo, sizeof(MaterialData) * MAX_RENDER_ENTITIES,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

                    vmaMapMemory(gAllocator, gFrames[i].material_data_ssbo.vma_allocation, &mapped_material_data_ptrs[i]);
                }

                {
                    VkDescriptorBufferInfo buffer_info{};
                    buffer_info.buffer = gFrames[i].view_proj_uniforms.handle;
                    buffer_info.offset = 0;
                    buffer_info.range = VK_WHOLE_SIZE;

                    VkWriteDescriptorSet descriptor_writes{};
                    descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor_writes.dstSet = gView_projection_sets[i];
                    descriptor_writes.dstBinding = 0;
                    descriptor_writes.dstArrayElement;
                    descriptor_writes.descriptorCount = 1;
                    descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    descriptor_writes.pImageInfo;
                    descriptor_writes.pBufferInfo = &buffer_info;
                    descriptor_writes.pTexelBufferView;
                    vkUpdateDescriptorSets(gDevice, 1, &descriptor_writes, 0, NULL);
                }

                {
                    VkDescriptorBufferInfo buffer_info{};
                    buffer_info.buffer = gFrames[i].draw_data_ssbo.handle;
                    buffer_info.offset = 0;
                    buffer_info.range = sizeof(ObjectData) * MAX_RENDER_ENTITIES;

                    VkWriteDescriptorSet descriptor_writes{};
                    descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor_writes.dstSet = gDraw_data_sets[i];
                    descriptor_writes.dstBinding = 0;
                    descriptor_writes.dstArrayElement;
                    descriptor_writes.descriptorCount = 1;
                    descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptor_writes.pImageInfo;
                    descriptor_writes.pBufferInfo = &buffer_info;
                    descriptor_writes.pTexelBufferView;
                    vkUpdateDescriptorSets(gDevice, 1, &descriptor_writes, 0, NULL);
                }

                {
                    VkDescriptorBufferInfo buffer_info{};
                    buffer_info.buffer = gFrames[i].material_data_ssbo.handle;
                    buffer_info.offset = 0;
                    buffer_info.range = sizeof(MaterialData) * MAX_RENDER_ENTITIES;

                    VkWriteDescriptorSet descriptor_writes{};
                    descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor_writes.dstSet = gMaterial_data_sets[i];
                    descriptor_writes.dstBinding = 0;
                    descriptor_writes.dstArrayElement;
                    descriptor_writes.descriptorCount = 1;
                    descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptor_writes.pImageInfo;
                    descriptor_writes.pBufferInfo = &buffer_info;
                    descriptor_writes.pTexelBufferView;
                    vkUpdateDescriptorSets(gDevice, 1, &descriptor_writes, 0, NULL);
                }
            }
        }



        //
        // Bindless Texture descriptor
        //
        {
            VkDescriptorSetLayoutBinding desc_set_layout_binding_sampler = {};
            desc_set_layout_binding_sampler.binding = 0;
            desc_set_layout_binding_sampler.descriptorCount = 1;
            desc_set_layout_binding_sampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // cube sampler
            desc_set_layout_binding_sampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutBinding desc_set_layout_binding_sampled_image = {};
            desc_set_layout_binding_sampled_image.binding = 1;
            desc_set_layout_binding_sampled_image.descriptorCount = MAX_TEXTURE_COUNT; // texture count
            desc_set_layout_binding_sampled_image.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            desc_set_layout_binding_sampled_image.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            /// layout > array of textures & sampler
            std::vector<VkDescriptorSetLayoutBinding> array_of_textures_set_layout_bindings = {
                desc_set_layout_binding_sampler,
                desc_set_layout_binding_sampled_image
            };

            VkDescriptorSetLayoutCreateInfo create_info_desc_set_layout = {};
            create_info_desc_set_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            create_info_desc_set_layout.pNext = NULL;

            create_info_desc_set_layout.flags = 0;
            create_info_desc_set_layout.pBindings = array_of_textures_set_layout_bindings.data();
            create_info_desc_set_layout.bindingCount = (uint32_t)array_of_textures_set_layout_bindings.size();

            vkCreateDescriptorSetLayout(gDevice, &create_info_desc_set_layout, NULL, &gBindless_textures_set_layout);

            {
                // default Sampler
                VkSamplerCreateInfo ci_sampler = {};
                ci_sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                ci_sampler.minFilter = VK_FILTER_LINEAR;
                ci_sampler.magFilter = VK_FILTER_NEAREST;
                ci_sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                ci_sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                ci_sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                ci_sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                ci_sampler.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
                ci_sampler.maxLod = 10.f;
                ci_sampler.mipLodBias = 0.f;
                ci_sampler.compareOp = VK_COMPARE_OP_ALWAYS;

                //If the samplerAnisotropy feature is not enabled, anisotropyEnable must be VK_FALSE
                // todo: investigate anisotropy feature
                ci_sampler.anisotropyEnable = VK_TRUE;
                ci_sampler.maxAnisotropy = 8;
                VKCHECK(vkCreateSampler(gDevice, &ci_sampler, NULL, &gDefaultSampler));
            }



            // making space for future Texture uploads
            gDescriptor_image_infos.resize(MAX_TEXTURE_COUNT);
            memset(gDescriptor_image_infos.data(), 0, gDescriptor_image_infos.size() * sizeof(VkDescriptorImageInfo));

            for (size_t i = 0; i < MAX_TEXTURE_COUNT; i++)
            {
                gDescriptor_image_infos[i].sampler = gDefaultSampler;
            }


            VkDescriptorSetAllocateInfo desc_alloc_info{};
            desc_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            desc_alloc_info.descriptorPool = gDescriptor_pool;
            desc_alloc_info.descriptorSetCount = 1;
            desc_alloc_info.pSetLayouts = &gBindless_textures_set_layout;

            VKCHECK(vkAllocateDescriptorSets(gDevice, &desc_alloc_info, &gBindless_textures_set));


            // fixme: duplicate -> See Texture::Create(...)
            // gBindless_textures_set is written here because it needs to be "initialized"
            // before it can ever be used.
            // Then with each new texture that is created gBindless_textures_set is rewritten with an
            // updated gDescriptor_image_infos
            {
                VkWriteDescriptorSet setWrites[2]{};

                VkDescriptorImageInfo samplerInfo{};
                samplerInfo.sampler = gDefaultSampler;

                //setWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                //setWrites[0].dstSet = gBindless_textures_set;
                //setWrites[0].dstBinding = 0;
                //setWrites[0].dstArrayElement = 0;
                //setWrites[0].descriptorCount = 1;
                //setWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                //setWrites[0].pImageInfo = &samplerInfo;
                //setWrites[0].pBufferInfo = 0;

                setWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                setWrites[1].dstSet = gBindless_textures_set;
                setWrites[1].dstBinding = 1;
                setWrites[1].dstArrayElement = 0;
                setWrites[1].descriptorCount = (uint32_t)gDescriptor_image_infos.size();
                setWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                setWrites[1].pImageInfo = gDescriptor_image_infos.data();
                setWrites[1].pBufferInfo = 0;
                vkUpdateDescriptorSets(gDevice, 1, &setWrites[1], 0, NULL);
            }
        }

        //
        // Pipelines
        //
        CreateGraphicsPipeline(gDevice, &gDefault_graphics_pipeline);

        return 1;
    }

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

        WIDTH = surface_capabilities.currentExtent.width;
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
        swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_create_info.surface = surface;
        swapchain_create_info.minImageCount = _image_count;
        swapchain_create_info.imageFormat = surface_formats[0].format;
        swapchain_create_info.imageColorSpace = surface_formats[0].colorSpace;
        swapchain_create_info.imageExtent = surface_capabilities.currentExtent; // fixme: needs to be checked
        swapchain_create_info.imageArrayLayers = 1; // we are not doing stereo rendering
        swapchain_create_info.imageUsage = usage_flag;
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // only ever using swapchain _images with the same queue family
        swapchain_create_info.queueFamilyIndexCount = 1;
        swapchain_create_info.pQueueFamilyIndices = NULL;
        swapchain_create_info.preTransform = surface_capabilities.currentTransform; // fixme: we sure?
        swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // we dont want to blend our window with other windows
        swapchain_create_info.presentMode = present_mode;
        swapchain_create_info.clipped = VK_TRUE; // we don't care about pixels covered by another window
        swapchain_create_info.oldSwapchain = old_swapchain;

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
        // swapchain image views
        //
        _image_views.resize(_image_count);

        for (size_t i = 0; i < _image_count; i++) {
            VkImageViewCreateInfo view_ci = {};
            view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_ci.image = _images[i];
            view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_ci.format = _format;

            view_ci.components.r = VK_COMPONENT_SWIZZLE_R;
            view_ci.components.g = VK_COMPONENT_SWIZZLE_G;
            view_ci.components.b = VK_COMPONENT_SWIZZLE_B;
            view_ci.components.a = VK_COMPONENT_SWIZZLE_A;
            view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_ci.subresourceRange.baseMipLevel = 0;
            view_ci.subresourceRange.levelCount = 1;
            view_ci.subresourceRange.baseArrayLayer = 0;
            view_ci.subresourceRange.layerCount = 1;
            VKCHECK(vkCreateImageView(device, &view_ci, NULL, &_image_views[i]));
        }

        //
        // MSAA image
        //

        if (_msaa_image.handle != VK_NULL_HANDLE && old_swapchain != VK_NULL_HANDLE) {
            vkDestroyImageView(device, _msaa_image_view, nullptr);
            vmaDestroyImage(gAllocator, _msaa_image.handle, _msaa_image.vma_allocation);
        }

        VkImageCreateInfo resolve_image_ci{};
        resolve_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        resolve_image_ci.imageType = VK_IMAGE_TYPE_2D;
        resolve_image_ci.format = _format;

        resolve_image_ci.extent.width = WIDTH;
        resolve_image_ci.extent.height = HEIGHT;
        resolve_image_ci.extent.depth = 1;

        resolve_image_ci.mipLevels = 1;
        resolve_image_ci.arrayLayers = 1;
        resolve_image_ci.samples = sample_count;
        resolve_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL; // use VK_IMAGE_TILING_LINEAR if CPU writes are intended
        resolve_image_ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        resolve_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // resolve_image_ci.queueFamilyIndexCount;
        // resolve_image_ci.pQueueFamilyIndices;
        resolve_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo vma_msaa_image_alloc_ci{};
        vma_msaa_image_alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vma_msaa_image_alloc_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VKCHECK(vmaCreateImage(gAllocator, &resolve_image_ci, &vma_msaa_image_alloc_ci, &_msaa_image.handle, &_msaa_image.vma_allocation, NULL));

        VkImageViewCreateInfo resolve_view_ci{};
        resolve_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        resolve_view_ci.image = _msaa_image.handle;
        resolve_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        resolve_view_ci.format = _format;

        resolve_view_ci.components.r = VK_COMPONENT_SWIZZLE_R;
        resolve_view_ci.components.g = VK_COMPONENT_SWIZZLE_G;
        resolve_view_ci.components.b = VK_COMPONENT_SWIZZLE_B;
        resolve_view_ci.components.a = VK_COMPONENT_SWIZZLE_A;
        resolve_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        resolve_view_ci.subresourceRange.levelCount = 1;
        resolve_view_ci.subresourceRange.layerCount = 1;
        VKCHECK(vkCreateImageView(device, &resolve_view_ci, NULL, &_msaa_image_view));






        //
        // Depth image
        //

        if (_depth_image.handle != VK_NULL_HANDLE && old_swapchain != VK_NULL_HANDLE) {
            vkDestroyImageView(device, _depth_image_view, nullptr);
            vmaDestroyImage(gAllocator, _depth_image.handle, _depth_image.vma_allocation);
        }

        VkImageCreateInfo depth_image_ci{};
        depth_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depth_image_ci.imageType = VK_IMAGE_TYPE_2D;
        depth_image_ci.format = VK_FORMAT_D16_UNORM;

        depth_image_ci.extent.width = WIDTH;
        depth_image_ci.extent.height = HEIGHT;
        depth_image_ci.extent.depth = 1;

        depth_image_ci.mipLevels = 1;
        depth_image_ci.arrayLayers = 1;
        depth_image_ci.samples = sample_count;
        depth_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL; // use VK_IMAGE_TILING_LINEAR if CPU writes are intended
        depth_image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // depth_image_ci.queueFamilyIndexCount;
        // depth_image_ci.pQueueFamilyIndices;
        depth_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo vma_depth_alloc_ci{};
        vma_depth_alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vma_depth_alloc_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VKCHECK(vmaCreateImage(gAllocator, &depth_image_ci, &vma_depth_alloc_ci, &_depth_image.handle, &_depth_image.vma_allocation, NULL));

        VkImageViewCreateInfo depth_view_ci{};
        depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depth_view_ci.image = _depth_image.handle;
        depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depth_view_ci.format = VK_FORMAT_D16_UNORM;

        depth_view_ci.components.r = VK_COMPONENT_SWIZZLE_R;
        depth_view_ci.components.g = VK_COMPONENT_SWIZZLE_G;
        depth_view_ci.components.b = VK_COMPONENT_SWIZZLE_B;
        depth_view_ci.components.a = VK_COMPONENT_SWIZZLE_A;
        depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_view_ci.subresourceRange.levelCount = 1;
        depth_view_ci.subresourceRange.layerCount = 1;
        VKCHECK(vkCreateImageView(device, &depth_view_ci, NULL, &_depth_image_view));
    }

    bool CreateShaderModule(VkDevice device, const char* filepath, VkShaderModule* out_ShaderModule)
    {

        FILE* f = fopen(filepath, "rb");
        if (!f) {
            SDL_Log("Could not find path to %s : %s", filepath, strerror(errno));
            return NULL;
        }

        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        uint32_t* buffer = (uint32_t*)malloc(fsize);
        assert(buffer);
        fread(buffer, fsize, 1, f);
        fclose(f);

        // create a new shader module, using the buffer we loaded
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.codeSize = fsize;
        createInfo.pCode = buffer;

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
    static void CreateGraphicsPipeline(VkDevice device, VkPipeline* pipeline)
    {
        // [x]write shaders
        // [x]compile to spv
        // [x]load spv
        // [x]create modules
        // [x]build pipeline

        VkShaderModule vertex_shader_module;
        VkShaderModule fragment_shader_module;
        CreateShaderModule(device, "shaders/standard.vert.spv", &vertex_shader_module);
        CreateShaderModule(device, "shaders/standard.frag.spv", &fragment_shader_module);

        //
        // Pipeline
        //
        VkPipelineShaderStageCreateInfo shader_stage_cis[2]{};
        shader_stage_cis[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_cis[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stage_cis[0].module = vertex_shader_module;
        shader_stage_cis[0].pName = "main";

        shader_stage_cis[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_cis[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stage_cis[1].module = fragment_shader_module;
        shader_stage_cis[1].pName = "main";

        //
        // Raster state
        //
        VkPipelineRasterizationStateCreateInfo raster_state_ci{};
        raster_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster_state_ci.depthClampEnable;
        raster_state_ci.rasterizerDiscardEnable = VK_FALSE;
        raster_state_ci.polygonMode = VK_POLYGON_MODE_FILL;
        raster_state_ci.cullMode = VK_CULL_MODE_BACK_BIT;
        raster_state_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster_state_ci.depthBiasEnable;
        raster_state_ci.depthBiasConstantFactor;
        raster_state_ci.depthBiasClamp;
        raster_state_ci.depthBiasSlopeFactor;
        raster_state_ci.lineWidth = 1.f;


        //
        // Vertex input state - Non-interleaved, so we must bind each vertex attribute to a different slot
        //
        const int                       BINDING_COUNT = 6;
        VkVertexInputBindingDescription vertex_binding_descriptions[BINDING_COUNT]{ 0 };
        vertex_binding_descriptions[0].binding = 0; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[0].stride = sizeof(float) * 3;
        vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[1].binding = 1; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[1].stride = sizeof(float) * 3;
        vertex_binding_descriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[2].binding = 2; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[2].stride = sizeof(float) * 2;
        vertex_binding_descriptions[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[3].binding = 3; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[3].stride = sizeof(float) * 2;
        vertex_binding_descriptions[3].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[4].binding = 4; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[4].stride = sizeof(uint8_t) * 4;
        vertex_binding_descriptions[4].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertex_binding_descriptions[5].binding = 5; // corresponds to vkCmdBindVertexBuffer(binding)
        vertex_binding_descriptions[5].stride = sizeof(float) * 4;
        vertex_binding_descriptions[5].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


        const int                         ATTRIB_COUNT = 6;
        VkVertexInputAttributeDescription vertex_attrib_descriptions[ATTRIB_COUNT]{ 0 };
        vertex_attrib_descriptions[0].location = 0;
        vertex_attrib_descriptions[0].binding = 0;
        vertex_attrib_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_attrib_descriptions[0].offset = 0; // 0: tightly packed

        vertex_attrib_descriptions[1].location = 1;
        vertex_attrib_descriptions[1].binding = 1;
        vertex_attrib_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_attrib_descriptions[1].offset = 0;

        vertex_attrib_descriptions[2].location = 2;
        vertex_attrib_descriptions[2].binding = 2;
        vertex_attrib_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        vertex_attrib_descriptions[2].offset = 0;

        vertex_attrib_descriptions[3].location = 3;
        vertex_attrib_descriptions[3].binding = 3;
        vertex_attrib_descriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        vertex_attrib_descriptions[3].offset = 0;

        vertex_attrib_descriptions[4].location = 4;
        vertex_attrib_descriptions[4].binding = 4;
        vertex_attrib_descriptions[4].format = VK_FORMAT_R8G8B8A8_UNORM;
        vertex_attrib_descriptions[4].offset = 0;

        vertex_attrib_descriptions[5].location = 5;
        vertex_attrib_descriptions[5].binding = 5;
        vertex_attrib_descriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        vertex_attrib_descriptions[5].offset = 0;

        VkPipelineVertexInputStateCreateInfo vertex_input_state_ci{};
        vertex_input_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_ci.vertexBindingDescriptionCount = ARR_COUNT(vertex_binding_descriptions);
        vertex_input_state_ci.pVertexBindingDescriptions = vertex_binding_descriptions;
        vertex_input_state_ci.vertexAttributeDescriptionCount = ARR_COUNT(vertex_attrib_descriptions);
        vertex_input_state_ci.pVertexAttributeDescriptions = vertex_attrib_descriptions;

        //
        // Input assembly state
        //
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_ci{};
        input_assembly_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_ci.primitiveRestartEnable;

        //
        // Dynamic states
        //

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
            VK_DYNAMIC_STATE_CULL_MODE,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_ci{};
        dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_ci.dynamicStateCount = ARR_COUNT(dynamic_states);
        dynamic_state_ci.pDynamicStates = dynamic_states;

        //
        // Viewport state
        //
        VkPipelineViewportStateCreateInfo viewport_state_ci{};
        viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_ci.viewportCount = 1;
        viewport_state_ci.pViewports = NULL;
        viewport_state_ci.scissorCount = 1;
        viewport_state_ci.pScissors = NULL;




        //
        // Depth/Stencil state
        //
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_ci{};
        depth_stencil_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_ci.depthTestEnable = VK_TRUE; // tests wether fragment sould be discarded
        depth_stencil_state_ci.depthWriteEnable = VK_TRUE; // should depth test result be written to depth buffer?
        depth_stencil_state_ci.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // keeps fragments that are closer (lower depth)

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
        VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
        color_blend_attachment_state.blendEnable = VK_FALSE;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state_ci{};
        color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_ci.logicOpEnable = VK_FALSE;
        color_blend_state_ci.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_ci.attachmentCount = 1;
        color_blend_state_ci.pAttachments = &color_blend_attachment_state;
        color_blend_state_ci.blendConstants[0] = 1.f;
        color_blend_state_ci.blendConstants[1] = 1.f;
        color_blend_state_ci.blendConstants[2] = 1.f;
        color_blend_state_ci.blendConstants[3] = 1.f;



        //
        // Push Contstants
        //
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(PushConstants);


        //
        // Pipeline layout
        //

        std::vector<VkDescriptorSetLayout> set_layouts{
            gView_projection_set_layout,
            gDraw_data_set_layout,
            gMaterial_data_set_layout,
            gBindless_textures_set_layout
        };


        VkPipelineLayoutCreateInfo layout_ci{};
        layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_ci.setLayoutCount = (uint32_t)set_layouts.size();
        layout_ci.pSetLayouts = &set_layouts[0];
        layout_ci.pushConstantRangeCount = 1;
        layout_ci.pPushConstantRanges = &push_constant_range;

        vkCreatePipelineLayout(device, &layout_ci, NULL, &gDefault_graphics_pipeline_layout);


        //
        // Multisample state
        //
        VkPipelineMultisampleStateCreateInfo multisample_state_ci{};
        multisample_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state_ci.rasterizationSamples = sample_count; // fixme: hardoded
        multisample_state_ci.sampleShadingEnable = VK_FALSE;
        multisample_state_ci.minSampleShading;
        multisample_state_ci.pSampleMask;
        multisample_state_ci.alphaToCoverageEnable;
        multisample_state_ci.alphaToOneEnable;


        //
        // Pipeline state
        //
        VkPipelineRenderingCreateInfo rendering_ci{};
        rendering_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_ci.colorAttachmentCount = 1;
        rendering_ci.pColorAttachmentFormats = &gSwapchain._format;
        rendering_ci.depthAttachmentFormat = VK_FORMAT_D16_UNORM;

        VkGraphicsPipelineCreateInfo pipeline_ci{};
        pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_ci.pNext = &rendering_ci;
        pipeline_ci.stageCount = ARR_COUNT(shader_stage_cis);
        pipeline_ci.pStages = shader_stage_cis;
        pipeline_ci.pVertexInputState = &vertex_input_state_ci;
        pipeline_ci.pInputAssemblyState = &input_assembly_state_ci;
        pipeline_ci.pTessellationState;
        pipeline_ci.pViewportState = &viewport_state_ci;
        pipeline_ci.pRasterizationState = &raster_state_ci;
        pipeline_ci.pMultisampleState = &multisample_state_ci;
        pipeline_ci.pDepthStencilState = &depth_stencil_state_ci;
        pipeline_ci.pColorBlendState = &color_blend_state_ci;
        pipeline_ci.pDynamicState = &dynamic_state_ci;
        pipeline_ci.layout = gDefault_graphics_pipeline_layout;
        pipeline_ci.renderPass;
        pipeline_ci.subpass;
        pipeline_ci.basePipelineHandle;
        pipeline_ci.basePipelineIndex;

        VKCHECK(vkCreateGraphicsPipelines(device, 0, 1, &pipeline_ci, NULL, pipeline));
    }





    /////////////////////////////////
    // vulkan utility functions

    // Allocates one primary command buffer from global graphics command pool
    // assumed to handle transfer operations

    static VkCommandBuffer BeginCommandBuffer()
    {
        VkCommandBufferBeginInfo cmd_buffer_begin_info = {};
        cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        // fixme: we need thowaway command buffers
        auto command_buffer = gFrames[0].graphics_command_buffer;
        vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info);
        return command_buffer;
    }

    static void FlushCommandBuffer(const VkCommandBuffer* command_buffer)
    {
        VKCHECK(vkEndCommandBuffer(*command_buffer));
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = command_buffer;
        VKCHECK(vkQueueSubmit(gQueues._graphics, 1, &submit_info, VK_NULL_HANDLE));
        vkDeviceWaitIdle(gDevice);
    }

    static VkResult CreateBuffer(Buffer* buffer, VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage)
    {
        VkBufferCreateInfo buffer_ci{};
        buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_ci.size = size;
        buffer_ci.usage = buffer_usage;
        buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vma_allocation_ci{};
        vma_allocation_ci.flags = allocation_flags;
        vma_allocation_ci.usage = memory_usage;

        return vmaCreateBuffer(gAllocator, &buffer_ci, &vma_allocation_ci, &buffer->handle, &buffer->vma_allocation, NULL);
    }

    static void TransitionImageLayout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource_range, bool is_depth)
    {
        VkImageMemoryBarrier2 barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        barrier.image = image;
        barrier.subresourceRange = subresource_range;

        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
            if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            }
            else if (new_layout & (VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL | VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) && is_depth) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
            }
            else if (new_layout & (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL | VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) && !is_depth) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
        }
        else if (old_layout & (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL | VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)) {
            if (new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

                barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_NONE;
            }
        }

        else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }

            if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
        }
        else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
        }
        else {
            SDL_Log("Unsupported layout transition!");
            return;
        }

        VkDependencyInfo dependency_info{};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(command_buffer, &dependency_info);
    }

    void SetPushConstants(PushConstants* constants)
    {
        vkCmdPushConstants(gGraphics_cmd_buffer_in_flight, gDefault_graphics_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), constants);
    }

    void SetObjectData(ObjectData* object_data, uint32_t object_idx)
    {
        for (size_t i = 0; i < 128; i++) {
            ((ObjectData*)mapped_object_data_ptrs[gFrame_in_flight])[object_idx].joint_matrices[i] = object_data->joint_matrices[i];
        }
        ((ObjectData*)mapped_object_data_ptrs[gFrame_in_flight])[object_idx].model_matrix = object_data->model_matrix;
    }

    void SetWindowSize(int width, int height)
    {
        if (width <= 0 || height <= 0) {
            SDL_Log("width and height must be greater than 0");
            return;
        }

        WIDTH = width;
        HEIGHT = height;
    }

    void SetActiveCamera(Camera* camera)
    {
        if (!camera) {
            SDL_Log("trying to set NULL camera ptr");
            return;
        }

        gActive_camera = camera;
    }

    SDL_Window* GetSDLWindowHandle()
    {
        return gWindow;
    };

    void EnableDepthTest(bool enabled)
    {
        vkCmdSetDepthTestEnable(gGraphics_cmd_buffer_in_flight, enabled);
    }

    void EnableDepthWrite(bool enabled)
    {
        vkCmdSetDepthWriteEnable(gGraphics_cmd_buffer_in_flight, enabled);
    }

    void SetCullMode(VkCullModeFlags cullmode)
    {
        vkCmdSetCullMode(gGraphics_cmd_buffer_in_flight, cullmode);
    }
}
