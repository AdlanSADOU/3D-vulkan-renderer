#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

static std::unordered_map<VkResult, std::string>
vulkan_errors = {
	{ (VkResult)0, "VK_SUCCESS" },
	{ (VkResult)1, "VK_NOT_READY" },
	{ (VkResult)2, "VK_TIMEOUT" },
	{ (VkResult)3, "VK_EVENT_SET" },
	{ (VkResult)4, "VK_EVENT_RESET" },
	{ (VkResult)5, "VK_INCOMPLETE" },
	{ (VkResult)-1, "VK_ERROR_OUT_OF_HOST_MEMORY" },
	{ (VkResult)-2, "VK_ERROR_OUT_OF_DEVICE_MEMORY" },
	{ (VkResult)-3, "VK_ERROR_INITIALIZATION_FAILED" },
	{ (VkResult)-4, "VK_ERROR_DEVICE_LOST" },
	{ (VkResult)-5, "VK_ERROR_MEMORY_MAP_FAILED" },
	{ (VkResult)-6, "VK_ERROR_LAYER_NOT_PRESENT" },
	{ (VkResult)-7, "VK_ERROR_EXTENSION_NOT_PRESENT" },
	{ (VkResult)-8, "VK_ERROR_FEATURE_NOT_PRESENT" },
	{ (VkResult)-9, "VK_ERROR_INCOMPATIBLE_DRIVER" },
	{ (VkResult)-10, "VK_ERROR_TOO_MANY_OBJECTS" },
	{ (VkResult)-11, "VK_ERROR_FORMAT_NOT_SUPPORTED" },
	{ (VkResult)-12, "VK_ERROR_FRAGMENTED_POOL" },
	{ (VkResult)-13, "VK_ERROR_UNKNOWN" },
	{ (VkResult)-1000069000, "VK_ERROR_OUT_OF_POOL_MEMORY" },
	{ (VkResult)-1000072003, "VK_ERROR_INVALID_EXTERNAL_HANDLE" },
	{ (VkResult)-1000161000, "VK_ERROR_FRAGMENTATION" },
	{ (VkResult)-1000257000, "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS" },
	{ (VkResult)-1000000000, "VK_ERROR_SURFACE_LOST_KHR" },
	{ (VkResult)-1000000001, "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR" },
	{ (VkResult)1000001003, "VK_SUBOPTIMAL_KHR" },
	{ (VkResult)-1000001004, "VK_ERROR_OUT_OF_DATE_KHR" },
	{ (VkResult)-1000003001, "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR" },
	{ (VkResult)-1000011001, "VK_ERROR_VALIDATION_FAILED_EXT" },
	{ (VkResult)-1000012000, "VK_ERROR_INVALID_SHADER_NV" },
	{ (VkResult)-1000158000, "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT" },
	{ (VkResult)-1000174001, "VK_ERROR_NOT_PERMITTED_EXT" },
	{ (VkResult)-1000255000, "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT" },
	{ (VkResult)1000268000, "VK_THREAD_IDLE_KHR" },
	{ (VkResult)1000268001, "VK_THREAD_DONE_KHR" },
	{ (VkResult)1000268002, "VK_OPERATION_DEFERRED_KHR" },
	{ (VkResult)1000268003, "VK_OPERATION_NOT_DEFERRED_KHR" },
	{ (VkResult)1000297000, "VK_PIPELINE_COMPILE_REQUIRED_EXT" },
	{ VK_ERROR_OUT_OF_POOL_MEMORY, "VK_ERROR_OUT_OF_POOL_MEMORY_KHR" },
	{ VK_ERROR_INVALID_EXTERNAL_HANDLE, "VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR" },
	{ VK_ERROR_FRAGMENTATION, "VK_ERROR_FRAGMENTATION_EXT" },
	{ VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS, "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT" },
	{ VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS, "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR" },
	{ VK_PIPELINE_COMPILE_REQUIRED_EXT, "VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT" },
	{ (VkResult)0x7FFFFFFF, "VK_RESULT_MAX_ENUM" }
};

#if defined(_DEBUG)
#define VKCHECK(x)                                                                                      \
    do {                                                                                                \
        VkResult err = x;                                                                               \
        if (err) {                                                                                      \
            SDL_Log("%s:%d Detected Vulkan error: %s", __FILE__, __LINE__, vulkan_errors[err].c_str()); \
        }                                                                                               \
    } while (0)
#else
#define VKCHECK(x) x
#endif
