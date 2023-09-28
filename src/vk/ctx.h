#ifndef _INCLUDE_CTX_H
#define _INCLUDE_CTX_H
#include "hatpch.h"

#include "allocator.h"
#include "deleter.h"
#include "upload_context.h"

namespace hatgpu
{
namespace vk
{
struct Ctx
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties gpuProperties;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;

    vk::Allocator allocator;
    vk::UploadContext uploadContext;
};
}  // namespace vk
}  // namespace hatgpu

#endif
