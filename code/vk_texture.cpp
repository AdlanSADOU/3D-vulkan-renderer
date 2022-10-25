void Texture::Create(const char *filepath)
{
    id = textures_count;

    int tex_width, tex_height, texChannels;

    // STBI_rgb_alpha forces alpha
    stbi_uc *pixels = stbi_load(filepath, &tex_width, &tex_height, &texChannels, STBI_rgb_alpha);
    if (!pixels)
        SDL_Log("failed to load %s", filepath);

    size_t imageSize = tex_width * tex_height * 4;

    width        = tex_width;
    height       = tex_height;
    num_channels = texChannels;
    format       = VK_FORMAT_R8G8B8A8_UNORM;


    VkImageCreateInfo ci_image = {};
    ci_image.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci_image.imageType         = VK_IMAGE_TYPE_2D;
    ci_image.format            = VK_FORMAT_R8G8B8A8_UNORM;
    ci_image.extent            = { (uint32_t)width, (uint32_t)height, 1 };
    ci_image.mipLevels         = 1;
    ci_image.arrayLayers       = 1;
    ci_image.samples           = VK_SAMPLE_COUNT_1_BIT;
    ci_image.usage             = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci_image.tiling            = VK_IMAGE_TILING_OPTIMAL; // 0
    ci_image.sharingMode       = VK_SHARING_MODE_EXCLUSIVE; // 0
    ci_image.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED; // 0

    VmaAllocationCreateInfo ci_allocation = {};
    ci_allocation.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
    VKCHECK(vmaCreateImage(gAllocator, &ci_image, &ci_allocation, &image, &allocation, NULL));


    VkImageViewCreateInfo ci_image_view = {};
    ci_image_view.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci_image_view.image                 = image;
    ci_image_view.viewType              = VK_IMAGE_VIEW_TYPE_2D;
    ci_image_view.format                = VK_FORMAT_R8G8B8A8_UNORM;
    ci_image_view.components            = {}; // VK_COMPONENT_SWIZZLE_IDENTITY = 0
    ci_image_view.subresourceRange      = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    VKCHECK(vkCreateImageView(gDevice, &ci_image_view, NULL, &view));


    Buffer staging_buffer;
    VKCHECK(CreateBuffer(&staging_buffer, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT, VMA_MEMORY_USAGE_CPU_ONLY));

    void *staging_data;
    vmaMapMemory(gAllocator, staging_buffer.vma_allocation, &staging_data);
    memcpy(staging_data, pixels, imageSize);
    vmaFlushAllocation(gAllocator, staging_buffer.vma_allocation, 0, VK_WHOLE_SIZE);
    vmaUnmapMemory(gAllocator, staging_buffer.vma_allocation);

    stbi_image_free(pixels);


    // begin staging command buffer
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {};
    cmd_buffer_begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(gFrames[0].graphics_command_buffer, &cmd_buffer_begin_info);

    // image layout transitioning
    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresource_range.baseMipLevel   = 0;
    image_subresource_range.levelCount     = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount     = 1;

    VkImageMemoryBarrier image_memory_barrier_from_undefined_to_transfer_dst = {};
    image_memory_barrier_from_undefined_to_transfer_dst.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier_from_undefined_to_transfer_dst.srcAccessMask        = 0;
    image_memory_barrier_from_undefined_to_transfer_dst.dstAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barrier_from_undefined_to_transfer_dst.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
    image_memory_barrier_from_undefined_to_transfer_dst.newLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barrier_from_undefined_to_transfer_dst.image                = image;
    image_memory_barrier_from_undefined_to_transfer_dst.subresourceRange     = image_subresource_range;

    {
        VkPipelineStageFlags src_stage_flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dst_stage_flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        vkCmdPipelineBarrier(gFrames[0].graphics_command_buffer, src_stage_flags, dst_stage_flags, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier_from_undefined_to_transfer_dst);
    }

    VkBufferImageCopy buffer_image_copy;
    buffer_image_copy.bufferOffset      = 0;
    buffer_image_copy.bufferRowLength   = 0;
    buffer_image_copy.bufferImageHeight = 0;
    buffer_image_copy.imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    buffer_image_copy.imageOffset       = { 0, 0, 0 }; // x, y, z
    buffer_image_copy.imageExtent       = { (uint32_t)tex_width, (uint32_t)tex_height, 1 };
    vkCmdCopyBufferToImage(gFrames[0].graphics_command_buffer, staging_buffer.handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

    // Image layout transfer to SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier image_memory_barrier_from_transfer_to_shader_read = {};
    image_memory_barrier_from_transfer_to_shader_read.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier_from_transfer_to_shader_read.srcAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barrier_from_transfer_to_shader_read.dstAccessMask        = VK_ACCESS_SHADER_READ_BIT;
    image_memory_barrier_from_transfer_to_shader_read.oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_memory_barrier_from_transfer_to_shader_read.newLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_memory_barrier_from_transfer_to_shader_read.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier_from_transfer_to_shader_read.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier_from_transfer_to_shader_read.image                = image;
    image_memory_barrier_from_transfer_to_shader_read.subresourceRange     = image_subresource_range;

    VkPipelineStageFlags src_stage_flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkPipelineStageFlags dst_stage_flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    vkCmdPipelineBarrier(gFrames[0].graphics_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier_from_transfer_to_shader_read);

    VKCHECK(vkEndCommandBuffer(gFrames[0].graphics_command_buffer));


    // Submit command buffer and copy data from staging buffer to a vertex buffer
    VkSubmitInfo submit_info {};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &gFrames[0].graphics_command_buffer;
    VKCHECK(vkQueueSubmit(gQueues._graphics, 1, &submit_info, VK_NULL_HANDLE));

    vkDeviceWaitIdle(gDevice);
    vmaDestroyBuffer(gAllocator, staging_buffer.handle, staging_buffer.vma_allocation);


    VkDescriptorImageInfo desc_image_info {};
    desc_image_info.sampler                 = NULL;
    desc_image_info.imageLayout             = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    desc_image_info.imageView               = view;
    gDescriptor_image_infos[textures_count] = desc_image_info;

    VkWriteDescriptorSet setWrites[2] {};

    VkDescriptorImageInfo samplerInfo {};
    samplerInfo.sampler = gDefaultSampler;

    setWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    setWrites[0].dstSet          = gBindless_textures_set;
    setWrites[0].dstBinding      = 0;
    setWrites[0].dstArrayElement = 0;
    setWrites[0].descriptorCount = 1;
    setWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    setWrites[0].pImageInfo      = &samplerInfo;
    setWrites[0].pBufferInfo     = 0;

    setWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    setWrites[1].dstSet          = gBindless_textures_set;
    setWrites[1].dstBinding      = 1;
    setWrites[1].dstArrayElement = 0;
    setWrites[1].descriptorCount = (uint32_t)gDescriptor_image_infos.size();
    setWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    setWrites[1].pImageInfo      = gDescriptor_image_infos.data();
    setWrites[1].pBufferInfo     = 0;
    vkUpdateDescriptorSets(gDevice, 2, setWrites, 0, NULL);


    textures_count++;
}

void Texture::Destroy()
{
    vkDestroyImageView(gDevice, view, NULL);
    vmaDestroyImage(gAllocator, image, allocation);
}

static void LoadAndCacheTexture(int32_t *texture_idx, const char *texture_uri, const char *folder_path)
{
    std::string full_path;
    full_path.append(folder_path);
    full_path.append(texture_uri);

    Texture *t = (Texture *)malloc(sizeof(Texture));
    t->Create(full_path.c_str());
    t->name = strdup(texture_uri);

    gTextures.insert({ std::string(texture_uri), t });

    *texture_idx = t->id;
}
