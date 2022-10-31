#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLM/glm.hpp>
#include <GLM/gtx/quaternion.hpp>
#include <GLM/gtc/matrix_transform.hpp>

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#define _VMA_PUBLIC_INTERFACE
#include <vk_mem_alloc.h>

#include "vk_types.h"
#include "camera.h"
#include "animation.h"
#include "vk_texture.h"
#include "vk_mesh.h"

void SetPushConstants(PushConstants *constants);
void SetObjectData(ObjectData *object_data, uint32_t object_idx);
void SetWindowDimensions(int width, int height);
void SetActiveCamera(Camera *camera);