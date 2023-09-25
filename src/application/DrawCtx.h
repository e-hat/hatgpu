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
    vk::Ctx vk;
    VkImageView swapchainImageView;
    VkImage swapchainImage;
};

// Tracy helpers
#define VkZoneC(name, color) \
    TracyVkZoneC(mCurrentDrawCtx->tracyCtx, mCurrentDrawCtx->commandBuffer, name, color)
}  // namespace hatgpu

#endif
