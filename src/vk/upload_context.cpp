#include "hatpch.h"

#include "vk/deleter.h"
#include "vk/initializers.h"
#include "vk/upload_context.h"

#include <iostream>

namespace hatgpu
{
namespace vk
{
UploadContext::UploadContext(VkDevice device, VkQueue graphicsQueue, uint32_t graphicsQueueIndex)
    : device(device), graphicsQueue(graphicsQueue)
{
    VkFenceCreateInfo uploadFenceCreateInfo = vk::fenceInfo();
    H_CHECK(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadFence),
            "Failed to create upload context fence");

    VkCommandPoolCreateInfo commandPoolCreateInfo = vk::commandPoolInfo(graphicsQueueIndex);
    H_CHECK(vkCreateCommandPool(this->device, &commandPoolCreateInfo, nullptr, &commandPool),
            "Failed to create upload context command pool");

    VkCommandBufferAllocateInfo commandBufferAllocInfo = vk::commandBufferAllocInfo(commandPool);
    H_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocInfo, &commandBuffer),
            "Failed to allocate upload context command buffer");
}

void UploadContext::immediateSubmit(std::function<void(VkCommandBuffer)> &&function)
{
    VkCommandBuffer cmd = commandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo =
        vk::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    H_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo), "Failed to begin command buffer");

    function(cmd);

    H_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");

    VkSubmitInfo submitInfo = vk::submitInfo(&cmd);
    H_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadFence), "Failed to submit to queue");

    vkWaitForFences(device, 1, &uploadFence, true, std::numeric_limits<uint32_t>::max());
    vkResetFences(device, 1, &uploadFence);

    vkResetCommandPool(device, commandPool, 0);
}

void UploadContext::destroy()
{
    H_LOG("...destroying upload context");
    vkDestroyFence(device, uploadFence, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
}
}  // namespace vk
}  // namespace hatgpu
