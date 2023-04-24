#ifndef _INCLUDED_UPLOAD_CONTEXT_H
#define _INCLUDED_UPLOAD_CONTEXT_H
#include "hatpch.h"

#include "vk/allocator.h"
#include "vk/deleter.h"

#include <functional>

namespace hatgpu
{
namespace vk
{
struct UploadContext
{
    UploadContext() = default;
    UploadContext(VkDevice device, VkQueue graphicsQueue, uint32_t graphicsQueueIndex);

    void immediateSubmit(std::function<void(VkCommandBuffer)> &&function);

    void destroy();

    VkFence uploadFence;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkDevice device;
    VkQueue graphicsQueue;
};
}  // namespace vk
}  // namespace hatgpu

#endif
