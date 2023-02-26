#pragma once

#include "common.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cgltf.h>
#include <SDL.h>
#include <vector>

#include "VKDevice.h"
#include "VKDeviceContext.h"
#include "VKTexture.h"


namespace FMK {


    const auto CULL_MODE_FRONT = VK_CULL_MODE_FRONT_BIT;
    const auto CULL_MODE_BACK = VK_CULL_MODE_BACK_BIT;

    FMK_API uint32_t WIDTH;
    FMK_API uint32_t HEIGHT;



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

    struct Transform
    {
        const char* mName = {};
        Transform* mParent = {};
        Transform* mChild = {};
        glm::vec3   mScale = {};
        glm::quat   mRotation = {};
        glm::vec3   mTranslation = {};

        Transform() {};

        Transform(glm::vec3 translation, glm::quat rotation, glm::vec3 scale)
        {
            mTranslation = translation;
            mRotation = rotation;
            mScale = scale;
        };

        FMK_API glm::mat4 GetLocalMatrix()
        {
            return glm::translate(glm::mat4(1), mTranslation)
                * glm::toMat4(mRotation)
                * glm::scale(glm::mat4(1), mScale);
        };
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







    struct Mesh
    {
        // todo: 
        // tying skeletal info to Mesh is probably a bad idea
        // but we'll think about this when we actually get to the
        // part we need to share skeletons and animations
        std::vector<Joint> _joints;
        std::vector<Animation> _animations{};
        int32_t _current_animation_idx = {};
        bool _should_play_animation = true;

        ObjectData    object_data;
        PushConstants push_constants;

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

    void CreateGraphicsPipeline(VkDevice device, VkPipeline* pipeline);
    FMK_API bool InitRenderer(int width, int height);

    FMK_API void RebuildGraphicsPipeline();

    FMK_API void BeginRendering();
    FMK_API void EndRendering();
    FMK_API void EnableDepthTest(bool enabled);
    FMK_API void EnableDepthWrite(bool enabled);
    FMK_API void SetCullMode(VkCullModeFlags cullmode);

    FMK_API void AnimationUpdate(Animation* anim, float dt);
    FMK_API SDL_Window* GetSDLWindowHandle();
    FMK_API void SetWindowSize(int width, int height);
    FMK_API void GetWindowSize(int* width, int* height);
    FMK_API void SetActiveCamera(Camera* camera);
    FMK_API void SetObjectData(ObjectData* object_data, uint32_t object_idx);
    FMK_API void SetPushConstants(PushConstants* constants);
    FMK_API void Draw(const Mesh* model);

    FMK_API void DeviceWaitIdle();

}
