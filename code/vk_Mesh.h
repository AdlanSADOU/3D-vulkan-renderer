#pragma once

struct SkinnedMesh
{
    void Create();
};

void SkinnedMesh::Create()
{
    VkBuffer      indirect_commands_buffer;
    VmaAllocation vma_allocation;

    {
        VkDrawIndexedIndirectCommand commands[1] {};
        commands->firstIndex    = 0;
        commands->firstInstance = 0;
        commands->indexCount    = ARR_COUNT(triangle_indices);
        commands->instanceCount = 1;
        commands->vertexOffset  = 0; // vertex data offset into buffer

        VkBufferCreateInfo buffer_ci {};
        buffer_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer_ci.size        = sizeof(commands);
        buffer_ci.usage       = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        VmaAllocationCreateInfo vma_alloc_ci {};
        vma_alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vma_alloc_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        vmaCreateBuffer(gAllocator, &buffer_ci, &vma_alloc_ci, &indirect_commands_buffer, &vma_allocation, NULL);

        void *indirect_commands_buffer_ptr;
        vmaMapMemory(gAllocator, vma_allocation, &indirect_commands_buffer_ptr);
        memcpy(indirect_commands_buffer_ptr, commands, sizeof(commands));
        vmaUnmapMemory(gAllocator, vma_allocation);
    }
}