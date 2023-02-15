#include "VKBuffer.h"


VkResult CreateBuffer(Buffer* buffer, VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage, VmaAllocator allocator)
{
    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = size;
    buffer_ci.usage = buffer_usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_allocation_ci{};
    vma_allocation_ci.flags = allocation_flags;
    vma_allocation_ci.usage = memory_usage;

    return vmaCreateBuffer(allocator, &buffer_ci, &vma_allocation_ci, &buffer->handle, &buffer->vma_allocation, NULL);
}