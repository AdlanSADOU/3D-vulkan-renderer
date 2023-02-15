#pragma once
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

struct Image
{
    VkImage       handle;
    VmaAllocation vma_allocation;
};

void TransitionImageLayout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource_range, bool is_depth);
