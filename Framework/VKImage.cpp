
#include "VKImage.h"
#include <SDL_log.h>

void TransitionImageLayout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource_range, bool is_depth)
{
    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange = subresource_range;

    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
        if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        }
        else if (new_layout & (VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL | VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) && is_depth) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        }
        else if (new_layout & (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL | VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) && !is_depth) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
    }
    else if (old_layout & (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL | VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)) {
        if (new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_NONE;
        }
    }

    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }

        if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
    }
    else {
        SDL_Log("Unsupported layout transition!");
        return;
    }

    VkDependencyInfo dependency_info{};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}