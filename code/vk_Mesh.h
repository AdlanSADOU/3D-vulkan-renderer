#pragma once

struct SkinnedModel
{
    VkBuffer      indirect_commands_buffer;
    VmaAllocation indirect_commands_vma_allocation;

    VkBuffer      vertex_buffer;
    VmaAllocation vertex_buffer_allocation;

    cgltf_data *data;

    struct SkinnedMesh
    {
        std::vector<VkDeviceSize> positions_offset;
        std::vector<VkDeviceSize> normals_offset;
        std::vector<VkDeviceSize> index_offset;
        std::vector<uint32_t>     index_count;
    };

    std::vector<SkinnedMesh> meshes;

    void Create(const char *path);
    void Draw(VkCommandBuffer command_buffer);
};

void SkinnedModel::Create(const char *path)
{
    ////////////////////////////////////////////
    //
    // Vertex & Index Buffer

    // create buffer
    // allocate memory
    LoadCgltfData(path, &data);





    // size_t vertex_buffer_size = sizeof(triangle_vertices) + sizeof(triangle_indices);
    size_t vertex_buffer_size = data->buffers->size;


    VkBufferCreateInfo buffer_ci {};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size  = vertex_buffer_size;
    buffer_ci.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_allocation_ci {};
    vma_allocation_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    vma_allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vmaCreateBuffer(gAllocator, &buffer_ci, &vma_allocation_ci, &vertex_buffer, &vertex_buffer_allocation, NULL);
    void *mapped_vertex_buffer_ptr;
    vmaMapMemory(gAllocator, vertex_buffer_allocation, &mapped_vertex_buffer_ptr);
    memcpy(mapped_vertex_buffer_ptr, data->buffers->data, vertex_buffer_size);
    vmaUnmapMemory(gAllocator, vertex_buffer_allocation);



    meshes.resize(data->meshes_count);


    for (size_t mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++) {
        meshes[mesh_idx].positions_offset.resize(data->meshes[mesh_idx].primitives_count);
        meshes[mesh_idx].normals_offset.resize(data->meshes[mesh_idx].primitives_count);
        meshes[mesh_idx].index_offset.resize(data->meshes[mesh_idx].primitives_count);
        meshes[mesh_idx].index_count.resize(data->meshes[mesh_idx].primitives_count);

        for (size_t submesh_idx = 0; submesh_idx < data->meshes[mesh_idx].primitives_count; submesh_idx++) {
            cgltf_primitive *primitive = &data->meshes[mesh_idx].primitives[submesh_idx];

            for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                cgltf_attribute   *attrib = &primitive->attributes[attrib_idx];
                cgltf_buffer_view *view   = attrib->data->buffer_view;

                if (attrib->type == cgltf_attribute_type_position)
                    meshes[mesh_idx].positions_offset[submesh_idx] = view->offset;
                if (attrib->type == cgltf_attribute_type_normal)
                    meshes[mesh_idx].normals_offset[submesh_idx] = view->offset;
            }


            meshes[mesh_idx].index_offset[submesh_idx] = primitive->indices->buffer_view->offset;
            meshes[mesh_idx].index_count[submesh_idx]  = primitive->indices->count;
        }
    }
}

void SkinnedModel::Draw(VkCommandBuffer command_buffer)
{
    VkBuffer buffers[] = {
        vertex_buffer,
        vertex_buffer,
    };

    for (size_t mesh_idx = 0; mesh_idx < meshes.size(); mesh_idx++) {

        for (size_t submesh_idx = 0; submesh_idx < meshes[mesh_idx].index_count.size(); submesh_idx++) {

            VkDeviceSize offsets[] = {
                /*POSITIONs*/ meshes[mesh_idx].positions_offset[submesh_idx],
                /*COLORs   */ meshes[mesh_idx].normals_offset[submesh_idx], // offset to the start of the attribute within buffer
            };

            vkCmdBindVertexBuffers(command_buffer, 0, ARR_COUNT(buffers), buffers, offsets);

            // vkCmdDrawIndexedIndirect(command_buffer, indirect_commands_buffer, 0, ARR_COUNT(commands), sizeof(commands[0]));
            vkCmdBindIndexBuffer(command_buffer, vertex_buffer, meshes[mesh_idx].index_offset[submesh_idx], VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(command_buffer, meshes[mesh_idx].index_count[submesh_idx], 1, 0, 0, 0);
        }
    }
}