#pragma once
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

struct Buffer
{
    VkBuffer      handle;
    VmaAllocation vma_allocation;
};

VkResult CreateBuffer(
    Buffer* buffer,
    VkDeviceSize size,
    VkBufferUsageFlags buffer_usage,
    VmaAllocationCreateFlags allocation_flags,
    VmaMemoryUsage memory_usage,
    VmaAllocator allocator);
