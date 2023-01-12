#pragma once

#include <GLM/glm.hpp>
#include <GLM/gtx/quaternion.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <cgltf.h>
#include <SDL2/SDL.h>
#include <vector>

#ifdef FRAMEWORK_EXPORTS
#define FMK_API __declspec(dllexport)
#else
#define FMK_API __declspec(dllimport)
#endif


namespace FMK {
    typedef unsigned char i8;


    const auto CULL_MODE_FRONT = VK_CULL_MODE_FRONT_BIT;
    const auto CULL_MODE_BACK = VK_CULL_MODE_BACK_BIT;
    const i8 MAX_JOINTS = 128u;
    const int MAX_RENDER_ENTITIES = 1024*10;

    FMK_API uint32_t WIDTH;
    FMK_API uint32_t HEIGHT;


    struct GlobalUniforms
    {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec3 camera_position;
    };

    struct ObjectData
    {
        glm::mat4 model_matrix;
        glm::mat4 joint_matrices[MAX_JOINTS];
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

        FMK_API void CameraCreate(glm::vec3 position, float fov, float aspect, float near, float far);
        FMK_API void CameraUpdate(Input* input, float dt, glm::vec3 target);
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

        FMK_API void Create(const char* filepath);
        FMK_API void CreateCubemapKTX(const char* filepath, VkFormat format);
        FMK_API void Destroy();
    };

    struct Transform
    {
        const char* name = {};
        Transform* parent = {};
        Transform* child = {};
        glm::vec3   scale = {};
        glm::quat   rotation = {};
        glm::vec3   translation = {};


        FMK_API glm::mat4 GetLocalMatrix()
        {
            return glm::translate(glm::mat4(1), translation)
                * glm::toMat4(rotation)
                * glm::scale(glm::mat4(1), scale);
        }
    };


    //
    // Animation
    //
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
        void* handle = {}; // cgltf_animation
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
        // todo: 
        // tying skeletol info to Mesh is probably a bad idea
        // but we'll think about this when we actually get to the
        // part we need to share skeletons and animations
        std::vector<Joint> _joints;
        // std::vector<AnimationV2> _animations_v2;
        std::vector<Animation> _animations;
        Animation* _current_animation = {};
        bool _should_play_animation = true;


        // todo: 
        // Mesh should not contain actual buffer/memory handles 
        // we must eventually cache this data to avoid
        // creating GPU backed buffers for the same Mesh multiple times
        // we must assign unique ids for every unique mesh that is loaded from disk
        VkBuffer      vertex_buffer = {};
        VmaAllocation vertex_buffer_allocation = {};

        // fixme: dont need to store this member. 
        // removing it would allow us to remove cgltf from the public interface
        cgltf_data* _mesh_data = {}; 

        struct SkinnedSubMesh
        {
            // this id is passed as 'baseInstance' for every drawn mesh 
            // through vkCmdDrawIndexed(..., firstInstance)
            // which can be retrieved with gl_BaseInstance in the vertex shader
            // it is a way to identify a submesh within a Mesh so that we can retieve its material data
            uint32_t instance_id = {};

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
        std::vector<SkinnedSubMesh> _meshes;

        FMK_API void Create(const char* path);
    };

    // exposed for the platform layer to be able to 
    // reload shaders on the fly
    FMK_API extern VkPipeline       gDefault_graphics_pipeline;
    FMK_API extern VkDevice         gDevice;

    // experimental
    FMK_API void CreateGraphicsPipeline(VkDevice device, VkPipeline* pipeline);

    FMK_API bool InitVulkanRenderer();

    FMK_API void BeginRendering();
    FMK_API void EndRendering();
    FMK_API void EnableDepthTest(bool enabled);
    FMK_API void EnableDepthWrite(bool enabled);
    FMK_API void SetCullMode(VkCullModeFlags cullmode);

    FMK_API void AnimationUpdate(Animation* anim, float dt);
    FMK_API SDL_Window* GetSDLWindowHandle();
    FMK_API void SetWindowSize(int width, int height);
    FMK_API void SetActiveCamera(Camera* camera);
    FMK_API void SetObjectData(ObjectData* object_data, uint32_t object_idx);
    FMK_API void SetPushConstants(PushConstants* constants);
    FMK_API void Draw(const Mesh* model);

}
