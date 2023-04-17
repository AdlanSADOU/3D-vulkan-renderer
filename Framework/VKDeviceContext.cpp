
#include "VKDeviceContext.h"
#include "VKDevice.h"

#include <SDL_log.h>

extern VKDevice gDevice;

void VKDeviceContext::AddDescriptorImageInfo(VkDescriptorImageInfo desc_image_info, bool is_cubemap)
{
    descriptor_image_infos[texture_descriptor_counter] = desc_image_info;
    if(!is_cubemap) texture_descriptor_counter++;
}

 bool VKDeviceContext::Create()
{



    //
    // VMA allocator
    //
    {
        VmaAllocatorCreateInfo vma_allocator_ci{};
        vma_allocator_ci.instance = gDevice.instance;
        vma_allocator_ci.device = gDevice.device;
        vma_allocator_ci.physicalDevice = gDevice.physical_device;

        vmaCreateAllocator(&vma_allocator_ci, &allocator);
    }





    //
    // Swapchain
    //
    {
        swapchain.Create(gDevice.surface, VK_NULL_HANDLE);
        frames.resize(swapchain._image_count);
    }




    //
    // Command Pools
    //
    {
        VkCommandPoolCreateInfo command_pool_ci = {};
        command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_ci.queueFamilyIndex = gDevice.queues.graphics_queue_family_idx;
        VKCHECK(vkCreateCommandPool(gDevice.device, &command_pool_ci, NULL, &graphics_command_pool));



        //
        // Per frame objects
        //
        for (size_t i = 0; i < swapchain._image_count; i++) {
            VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
            command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_buffer_allocate_info.commandPool = graphics_command_pool;
            command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_buffer_allocate_info.commandBufferCount = 1;
            VKCHECK(vkAllocateCommandBuffers(gDevice.device, &command_buffer_allocate_info, &frames[i].graphics_command_buffer));

            VkFenceCreateInfo fence_ci = {};
            fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VKCHECK(vkCreateFence(gDevice.device, &fence_ci, NULL, &frames[i].render_fence));

            VkSemaphoreCreateInfo semaphoreCreateInfo = {};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreCreateInfo.pNext = NULL;
            semaphoreCreateInfo.flags = 0;

            VKCHECK(vkCreateSemaphore(gDevice.device, &semaphoreCreateInfo, NULL, &frames[i].present_semaphore));
            VKCHECK(vkCreateSemaphore(gDevice.device, &semaphoreCreateInfo, NULL, &frames[i].render_semaphore));
        }
    }



    {
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKCHECK(vkBeginCommandBuffer(frames[0].graphics_command_buffer, &begin_info));

        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        subresource_range.levelCount = 1;
        subresource_range.layerCount = 1;
        TransitionImageLayout(frames[0].graphics_command_buffer, swapchain._depth_image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, subresource_range, true);

        VKCHECK(vkEndCommandBuffer(frames[0].graphics_command_buffer));


        VkCommandBufferSubmitInfo command_buffer_submit_info = {};
        command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command_buffer_submit_info.pNext = NULL;
        command_buffer_submit_info.commandBuffer = frames[0].graphics_command_buffer;

        VkSubmitInfo2 submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.flags;
        submit_info.commandBufferInfoCount = 1;
        submit_info.pCommandBufferInfos = &command_buffer_submit_info;

        VKCHECK(vkQueueSubmit2(gDevice.queues.graphics, 1, &submit_info, NULL));
        vkDeviceWaitIdle(gDevice.device);
    }


    //
    // Descriptor pool
    //
    {
        // we are creating a pool that can potentially hold 16 descriptor sets that hold swapchain.image_count number of uniform buffer descriptors
        VkDescriptorPoolSize descriptor_pool_sizes[3]{};
        descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_pool_sizes[0].descriptorCount = swapchain._image_count * 2; // total amount of a given descriptor type accross all sets

        descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_pool_sizes[1].descriptorCount = swapchain._image_count * 4; // total amount of a given descriptor type accross all sets

        descriptor_pool_sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_pool_sizes[2].descriptorCount = MAX_TEXTURE_COUNT; // total amount of a given descriptor type accross all sets

        VkDescriptorPoolCreateInfo descriptor_pool_ci{};
        descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        descriptor_pool_ci.maxSets = swapchain._image_count * 4;
        descriptor_pool_ci.poolSizeCount = ARR_COUNT(descriptor_pool_sizes); // 1 PoolSize = TYPE count of descriptors
        descriptor_pool_ci.pPoolSizes = descriptor_pool_sizes;
        VKCHECK(vkCreateDescriptorPool(gDevice.device, &descriptor_pool_ci, NULL, &descriptor_pool));


        //
        // Descriptor set layouts
        //

        // set 0
        // the layout for the set at binding 0 that will reference an MVP uniform block
        VkDescriptorSetLayoutBinding view_projection_set_layout_binding{};
        view_projection_set_layout_binding.binding = 0;
        view_projection_set_layout_binding.descriptorCount = 1; // 1 instance of MVP block for this binding
        view_projection_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        view_projection_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        view_projection_sets.resize(swapchain._image_count);


        // set 1
        VkDescriptorSetLayoutBinding draw_data_set_layout_binding{};
        draw_data_set_layout_binding.binding = 0;
        draw_data_set_layout_binding.descriptorCount = 1; //
        draw_data_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        draw_data_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        draw_data_sets.resize(swapchain._image_count);

        // set 2
        VkDescriptorSetLayoutBinding material_data_set_layout_binding{};
        material_data_set_layout_binding.binding = 0;
        material_data_set_layout_binding.descriptorCount = 1; //
        material_data_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        material_data_set_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        material_data_sets.resize(swapchain._image_count);


        {
            VkDescriptorSetLayoutCreateInfo set_layouts_ci{};
            set_layouts_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            set_layouts_ci.pNext = NULL;
            set_layouts_ci.flags = 0;
            set_layouts_ci.bindingCount = 1;
            set_layouts_ci.pBindings = &view_projection_set_layout_binding;

            // we need <frame_count> descriptor sets that each reference 1 MVP uniform block for each frame in flight
            VKCHECK(vkCreateDescriptorSetLayout(gDevice.device, &set_layouts_ci, NULL, &view_projection_set_layout));
        }

        {
            VkDescriptorSetLayoutCreateInfo set_layouts_ci{};
            set_layouts_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            set_layouts_ci.pNext = NULL;
            set_layouts_ci.flags = 0;
            set_layouts_ci.bindingCount = 1;
            set_layouts_ci.pBindings = &draw_data_set_layout_binding;

            // we need <frame_count> descriptor sets that each reference 1 MVP uniform block for each frame in flight
            VKCHECK(vkCreateDescriptorSetLayout(gDevice.device, &set_layouts_ci, NULL, &draw_data_set_layout));
        }

        {
            VkDescriptorSetLayoutCreateInfo set_layouts_ci{};
            set_layouts_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            set_layouts_ci.pNext = NULL;
            set_layouts_ci.flags = 0;
            set_layouts_ci.bindingCount = 1;
            set_layouts_ci.pBindings = &material_data_set_layout_binding;

            // we need <frame_count> descriptor sets that each reference 1 MVP uniform block for each frame in flight
            VKCHECK(vkCreateDescriptorSetLayout(gDevice.device, &set_layouts_ci, NULL, &material_data_set_layout));
        }


        for (size_t i = 0; i < swapchain._image_count; i++) {
            {
                VkDescriptorSetAllocateInfo descriptor_set_alloc_info{};
                descriptor_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                descriptor_set_alloc_info.descriptorPool = descriptor_pool;
                descriptor_set_alloc_info.descriptorSetCount = 1;
                descriptor_set_alloc_info.pSetLayouts = &view_projection_set_layout;
                VKCHECK(vkAllocateDescriptorSets(gDevice.device, &descriptor_set_alloc_info, &view_projection_sets[i]));
            }

            {
                VkDescriptorSetAllocateInfo descriptor_set_alloc_info{};
                descriptor_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                descriptor_set_alloc_info.descriptorPool = descriptor_pool;
                descriptor_set_alloc_info.descriptorSetCount = 1;
                descriptor_set_alloc_info.pSetLayouts = &draw_data_set_layout;
                VKCHECK(vkAllocateDescriptorSets(gDevice.device, &descriptor_set_alloc_info, &draw_data_sets[i]));
            }

            {
                VkDescriptorSetAllocateInfo descriptor_set_alloc_info{};
                descriptor_set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                descriptor_set_alloc_info.descriptorPool = descriptor_pool;
                descriptor_set_alloc_info.descriptorSetCount = 1;
                descriptor_set_alloc_info.pSetLayouts = &material_data_set_layout;
                VKCHECK(vkAllocateDescriptorSets(gDevice.device, &descriptor_set_alloc_info, &material_data_sets[i]));
            }
        }
    }





    //
    // Shader uniforms & Descriptor set updates/writes
    //
    {
        mapped_view_proj_ptrs.resize(swapchain._image_count);
        mapped_object_data_ptrs.resize(swapchain._image_count);
        mapped_material_data_ptrs.resize(swapchain._image_count);

        for (size_t i = 0; i < swapchain._image_count; i++) {

            {
                VKCHECK(CreateBuffer(&frames[i].view_proj_uniforms, sizeof(GlobalUniforms),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, allocator));
                VKCHECK(vmaMapMemory(allocator, frames[i].view_proj_uniforms.vma_allocation, &mapped_view_proj_ptrs[i]));

                VKCHECK(CreateBuffer(&frames[i].draw_data_ssbo, sizeof(ObjectData) * MAX_RENDER_ENTITIES,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, allocator));
                VKCHECK(vmaMapMemory(allocator, frames[i].draw_data_ssbo.vma_allocation, &mapped_object_data_ptrs[i]));

                VKCHECK(CreateBuffer(&frames[i].material_data_ssbo, sizeof(MaterialData) * MAX_RENDER_ENTITIES, // note: an entity could have 1 or more materials
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, allocator));
                VKCHECK(vmaMapMemory(allocator, frames[i].material_data_ssbo.vma_allocation, &mapped_material_data_ptrs[i]));
            }

            {
                VkDescriptorBufferInfo buffer_info{};
                buffer_info.buffer = frames[i].view_proj_uniforms.handle;
                buffer_info.offset = 0;
                buffer_info.range = VK_WHOLE_SIZE;

                VkWriteDescriptorSet descriptor_writes{};
                descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor_writes.dstSet = view_projection_sets[i];
                descriptor_writes.dstBinding = 0;
                descriptor_writes.dstArrayElement;
                descriptor_writes.descriptorCount = 1;
                descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptor_writes.pImageInfo;
                descriptor_writes.pBufferInfo = &buffer_info;
                descriptor_writes.pTexelBufferView;
                vkUpdateDescriptorSets(gDevice.device, 1, &descriptor_writes, 0, NULL);
            }

            {
                VkDescriptorBufferInfo buffer_info{};
                buffer_info.buffer = frames[i].draw_data_ssbo.handle;
                buffer_info.offset = 0;
                buffer_info.range = sizeof(ObjectData) * MAX_RENDER_ENTITIES;

                VkWriteDescriptorSet descriptor_writes{};
                descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor_writes.dstSet = draw_data_sets[i];
                descriptor_writes.dstBinding = 0;
                descriptor_writes.dstArrayElement;
                descriptor_writes.descriptorCount = 1;
                descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptor_writes.pImageInfo;
                descriptor_writes.pBufferInfo = &buffer_info;
                descriptor_writes.pTexelBufferView;
                vkUpdateDescriptorSets(gDevice.device, 1, &descriptor_writes, 0, NULL);
            }

            {
                VkDescriptorBufferInfo buffer_info{};
                buffer_info.buffer = frames[i].material_data_ssbo.handle;
                buffer_info.offset = 0;
                buffer_info.range = sizeof(MaterialData) * MAX_RENDER_ENTITIES;

                VkWriteDescriptorSet descriptor_writes{};
                descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor_writes.dstSet = material_data_sets[i];
                descriptor_writes.dstBinding = 0;
                descriptor_writes.dstArrayElement;
                descriptor_writes.descriptorCount = 1;
                descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptor_writes.pImageInfo;
                descriptor_writes.pBufferInfo = &buffer_info;
                descriptor_writes.pTexelBufferView;
                vkUpdateDescriptorSets(gDevice.device, 1, &descriptor_writes, 0, NULL);
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

        vkCreateDescriptorSetLayout(gDevice.device, &create_info_desc_set_layout, NULL, &bindless_textures_set_layout);

        {
            // default Sampler
            VkSamplerCreateInfo ci_sampler = {};
            ci_sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            ci_sampler.minFilter = VK_FILTER_LINEAR;
            ci_sampler.magFilter = VK_FILTER_LINEAR;
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
            VKCHECK(vkCreateSampler(gDevice.device, &ci_sampler, NULL, &defaultSampler));
        }



        // making space for future Texture uploads
        descriptor_image_infos.resize(MAX_TEXTURE_COUNT);
        memset(descriptor_image_infos.data(), 0, descriptor_image_infos.size() * sizeof(VkDescriptorImageInfo));

        for (size_t i = 0; i < MAX_TEXTURE_COUNT; i++)
        {
            descriptor_image_infos[i].sampler = defaultSampler;
        }


        VkDescriptorSetAllocateInfo desc_alloc_info{};
        desc_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc_alloc_info.descriptorPool = descriptor_pool;
        desc_alloc_info.descriptorSetCount = 1;
        desc_alloc_info.pSetLayouts = &bindless_textures_set_layout;

        VKCHECK(vkAllocateDescriptorSets(gDevice.device, &desc_alloc_info, &bindless_textures_set));


        // fixme: duplicate -> See Texture::Create(...)
        // bindless_textures_set is written here because it needs to be "initialized"
        // before it can ever be used.
        // Then with each new texture that is created bindless_textures_set is rewritten with an
        // updated descriptor_image_infos
        {
            VkWriteDescriptorSet setWrites[2]{};

            VkDescriptorImageInfo samplerInfo{};
            samplerInfo.sampler = defaultSampler;

            //setWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            //setWrites[0].dstSet = bindless_textures_set;
            //setWrites[0].dstBinding = 0;
            //setWrites[0].dstArrayElement = 0;
            //setWrites[0].descriptorCount = 1;
            //setWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            //setWrites[0].pImageInfo = &samplerInfo;
            //setWrites[0].pBufferInfo = 0;

            setWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            setWrites[1].dstSet = bindless_textures_set;
            setWrites[1].dstBinding = 1;
            setWrites[1].dstArrayElement = 0;
            setWrites[1].descriptorCount = (uint32_t)descriptor_image_infos.size();
            setWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            setWrites[1].pImageInfo = descriptor_image_infos.data();
            setWrites[1].pBufferInfo = 0;
            vkUpdateDescriptorSets(gDevice.device, 1, &setWrites[1], 0, NULL);
        }
    }

    return 1;
}


VkCommandBuffer VKDeviceContext::BeginCommandBuffer()
{
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    // fixme: we need thowaway command buffers
    auto command_buffer = frames[0].graphics_command_buffer;
    vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info);
    return command_buffer;
}

void VKDeviceContext::FlushCommandBuffer(const VkCommandBuffer* command_buffer)
{
    VKCHECK(vkEndCommandBuffer(*command_buffer));
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = command_buffer;
    VKCHECK(vkQueueSubmit(gDevice.queues.graphics, 1, &submit_info, VK_NULL_HANDLE));
    vkDeviceWaitIdle(gDevice.device);
}

