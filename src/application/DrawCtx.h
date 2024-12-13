#ifndef _INCLUDE_DRAW_CTX_H
#define _INCLUDE_DRAW_CTX_H
#include "hatpch.h"

#include "vk/ctx.h"

#include <tracy/TracyVulkan.hpp>

namespace hatgpu
{
struct DrawCtx
{
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    TracyVkCtx tracyCtx;
    std::shared_ptr<vk::Ctx> vk;
    VkImageView swapchainImageView;
    VkImage swapchainImage;
    VkImageView depthImageView;
    VkImage depthImage;

    uint32_t frameIndex;
};

// Tracy helpers
#define VkZoneC(name, color) TracyVkZoneC(drawCtx.tracyCtx, drawCtx.commandBuffer, name, color)
}  // namespace hatgpu

#endif
