#pragma once

#include <GLM/glm.hpp>
#include <GLM/gtx/quaternion.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <cgltf.h>
#include <vector>
#include <SDL2/SDL.h>

namespace Renderer {

    struct GlobalUniforms
    {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec3 camera_position;
    };

    struct ObjectData
    {
        glm::mat4 model_matrix;
        glm::mat4 joint_matrices[128];
    };

    struct PushConstants
    {
        int32_t object_idx = -1;
        int32_t has_joints = -1;
        int32_t is_skybox = -1;
    };

    struct Image
    {
        VkImage       handle;
        VmaAllocation vma_allocation;
    };


    struct Buffer
    {
        VkBuffer      handle;
        VmaAllocation vma_allocation;
    };

    struct Input
    {
        bool up;
        bool down;
        bool left;
        bool right;
        bool Q;
        bool E;

        struct Mouse
        {
            int32_t xrel;
            int32_t yrel;
            bool    left;
            bool    right;
        } mouse;
    };

    struct Camera
    {
        enum CameraMode {
            FREEFLY = 0,
            THIRD_PERSON
        } mode;


        glm::vec3 _position = {};
        glm::vec3 _up = {};
        glm::vec3 _right = {};
        glm::vec3 _forward = {};
        glm::vec3 _at = {};
        glm::mat4 _projection = {};
        glm::mat4 _view = {};
        float     _pitch = {};
        float     _yaw = {};
        float     _roll = {};
        float     _fov = {};
        float     _aspect = {};
        float     _near = {};
        float     _far = {};
        float     _speed = 60.f;
        float     _sensitivity = .046f;

        void CameraCreate(glm::vec3 position, float fov, float aspect, float near, float far);
        void CameraUpdate(Input* input, float dt, glm::vec3 target);
    };

    struct Texture
    {
        const char* name;
        VkImage image;
        VmaAllocation allocation;
        VkImageView view;
        VkFormat format;

        uint32_t mip_levels;
        uint32_t width;
        uint32_t height;
        uint32_t num_channels;

        // Shaders have an array of all textures (bindless textures)
        // this is the index into that array
        // see gDescriptor_image_infos & gBindless_textures_set
        uint32_t descriptor_array_idx;

        void Create(const char* filepath);
        void CreateCubemapKTX(const char* filepath, VkFormat format);
        void Destroy();
    };

    struct Transform
    {
        const char* name = {};
        Transform* parent = {};
        Transform* child = {};
        glm::vec3   scale = {};
        glm::quat   rotation = {};
        glm::vec3   translation = {};


        glm::mat4 GetLocalMatrix()
        {
            return glm::translate(glm::mat4(1), translation)
                * glm::toMat4(rotation)
                * glm::scale(glm::mat4(1), scale);
        }
    };


    //
    // Animation
    //
    const int MAX_JOINTS = 128;
    struct TranslationChannel
    {
        float* timestamps;
        glm::vec3* translations;
    };
    struct RotationChannel
    {
        float* timestamps;
        glm::quat* rotations;
    };
    struct ScaleChannel
    {
        float* timestamps;
        glm::vec3* scales;
    };

    struct Sample
    {
        uint8_t            target_joint;
        TranslationChannel translation_channel;
        RotationChannel    rotation_channel;
        ScaleChannel       scale_channel;
    };

    struct Joint
    {
        char* name;
        int8_t    parent;
        glm::mat4 inv_bind_matrix;
        glm::mat4 global_joint_matrix;
    };

    struct AnimationV2
    {
        float               duration;
        std::vector<Sample> samples;
    };

    struct Animation
    {
        void* handle = {};
        const char* name = {};
        float       duration = {};
        float       globalTimer = {};
        bool        isPlaying{};

        std::vector<Joint> _joints;

        glm::mat4 joint_matrices[MAX_JOINTS];
    };






    struct MaterialData
    {
        int32_t   base_color_texture_idx = -1;
        int32_t   emissive_texture_idx = -1;
        int32_t   normal_texture_idx = -1;
        int32_t   metallic_roughness_texture_idx = -1;
        float     tiling_x = 1.f;
        float     tiling_y = 1.f;
        float     metallic_factor;
        float     roughness_factor;
        glm::vec4 base_color_factor;
    };

    struct Mesh
    {
        std::vector<Joint> _joints;
        // std::vector<AnimationV2> _animations_v2;
        std::vector<Animation> _animations;
        Animation* _current_animation = {};
        bool _should_play_animation = true;


        // todo: we must eventually cache this data to avoid
        // creating GPU buffers for the same Mesh multiple times
        VkBuffer      vertex_buffer;
        VmaAllocation vertex_buffer_allocation;
        cgltf_data* _mesh_data; // fixme: dont need to store this member. removing it would allow us to remove cgltf from the public interface

        struct SkinnedMesh
        {
            // this id is passed as 'baseInstance' for every drawn mesh 
            // through vkCmdDrawIndexed(..., firstInstance)
            // which can be retrieved with gl_BaseInstance in the vertex shader
            // it is a way to identify a submesh within a Mesh so that we can retieve its material data
            uint32_t instance_id;

            std::vector<MaterialData> material_data{};

            // offsets to non-interleaved vertex attributes
            std::vector<VkDeviceSize> POSITION_offset;
            std::vector<VkDeviceSize> NORMAL_offset;
            std::vector<VkDeviceSize> TEXCOORD_0_offset;
            std::vector<VkDeviceSize> TEXCOORD_1_offset;
            std::vector<VkDeviceSize> JOINTS_0_offset;
            std::vector<VkDeviceSize> WEIGHTS_0_offset;

            std::vector<VkDeviceSize> index_offset;
            std::vector<uint32_t>     index_count;
        };
        std::vector<SkinnedMesh> _meshes;

        void Create(const char* path);
    };

    extern VkPipeline       gDefault_graphics_pipeline;
    extern VkDevice         gDevice;

    // experimental
    void CreateGraphicsPipeline(VkDevice device, VkPipeline* pipeline);

    bool InitRenderer();
    void SetWindowSize(int width, int height);

    void BeginRendering();
    void EndRendering();
    void EnableDepthTest(bool enabled);
    void EnableDepthWrite(bool enabled);
    void SetCullMode(VkCullModeFlags cullmode);

    void AnimationUpdate(Animation* anim, float dt);
    SDL_Window* GetSDLWindowHandle();
    void SetActiveCamera(Camera* camera);
    void SetObjectData(ObjectData* object_data, uint32_t object_idx);
    void SetWindowDimensions(int width, int height);
    void SetPushConstants(PushConstants* constants);
    void Draw(const Mesh* model);

}
