#pragma once

struct SkinnedModel
{
    Transform                _transform;
    PushConstants            _push_constants;


    // todo: we must eventually cache this data to avoid
    // creating GPU buffers for the same Mesh multiple times
    VkBuffer      vertex_buffer;
    VmaAllocation vertex_buffer_allocation;
    cgltf_data *_mesh_data;
    struct SkinnedMesh
    {
        std::vector<VkDeviceSize> positions_offset;
        std::vector<VkDeviceSize> normals_offset;
        std::vector<VkDeviceSize> index_offset;
        std::vector<uint32_t>     index_count;
    };
    std::vector<SkinnedMesh> _meshes;


    void Create(const char *path);
    void Draw(VkCommandBuffer command_buffer);
};

void SkinnedModel::Create(const char *path)
{
    if (gSharedMeshes.contains(path)) {
        _mesh_data = (cgltf_data *)gSharedMeshes[path];

    } else {
        cgltf_data *data;
        LoadCgltfData(path, &data);

        gSharedMeshes.insert({ std::string(path), (void *)data });
        _mesh_data = data;
    }





    // size_t vertex_buffer_size = sizeof(triangle_vertices) + sizeof(triangle_indices);
    size_t vertex_buffer_size = _mesh_data->buffers->size;


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
    memcpy(mapped_vertex_buffer_ptr, _mesh_data->buffers->data, vertex_buffer_size);
    vmaUnmapMemory(gAllocator, vertex_buffer_allocation);



    _meshes.resize(_mesh_data->meshes_count);


    for (size_t mesh_idx = 0; mesh_idx < _mesh_data->meshes_count; mesh_idx++) {
        _meshes[mesh_idx].positions_offset.resize(_mesh_data->meshes[mesh_idx].primitives_count);
        _meshes[mesh_idx].normals_offset.resize(_mesh_data->meshes[mesh_idx].primitives_count);
        _meshes[mesh_idx].index_offset.resize(_mesh_data->meshes[mesh_idx].primitives_count);
        _meshes[mesh_idx].index_count.resize(_mesh_data->meshes[mesh_idx].primitives_count);

        for (size_t submesh_idx = 0; submesh_idx < _mesh_data->meshes[mesh_idx].primitives_count; submesh_idx++) {
            cgltf_primitive *primitive = &_mesh_data->meshes[mesh_idx].primitives[submesh_idx];

            for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                cgltf_attribute   *attrib = &primitive->attributes[attrib_idx];
                cgltf_buffer_view *view   = attrib->data->buffer_view;

                if (attrib->type == cgltf_attribute_type_position)
                    _meshes[mesh_idx].positions_offset[submesh_idx] = view->offset;
                if (attrib->type == cgltf_attribute_type_normal)
                    _meshes[mesh_idx].normals_offset[submesh_idx] = view->offset;
            }


            _meshes[mesh_idx].index_offset[submesh_idx] = primitive->indices->buffer_view->offset;
            _meshes[mesh_idx].index_count[submesh_idx]  = primitive->indices->count;
        }
    }



}

void SkinnedModel::Draw(VkCommandBuffer command_buffer)
{


    VkBuffer buffers[] = {
        vertex_buffer,
        vertex_buffer,
    };

    for (size_t mesh_idx = 0; mesh_idx < _meshes.size(); mesh_idx++) {

        for (size_t submesh_idx = 0; submesh_idx < _meshes[mesh_idx].index_count.size(); submesh_idx++) {

            VkDeviceSize offsets[] = {
                /*POSITIONs*/ _meshes[mesh_idx].positions_offset[submesh_idx],
                /*COLORs   */ _meshes[mesh_idx].normals_offset[submesh_idx], // offset to the start of the attribute within buffer
            };

            vkCmdBindVertexBuffers(command_buffer, 0, ARR_COUNT(buffers), buffers, offsets);

            // vkCmdDrawIndexedIndirect(command_buffer, indirect_commands_buffer, 0, ARR_COUNT(commands), sizeof(commands[0]));
            vkCmdBindIndexBuffer(command_buffer, vertex_buffer, _meshes[mesh_idx].index_offset[submesh_idx], VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(command_buffer, _meshes[mesh_idx].index_count[submesh_idx], 1, 0, 0, 0);
        }
    }
}