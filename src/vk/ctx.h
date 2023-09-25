#ifndef _INCLUDE_CTX_H
#define _INCLUDE_CTX_H
#include "hatpch.h"

#include "deleter.h"

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

    vk::DeletionQueue mDeleter;
};
}  // namespace vk
}  // namespace hatgpu

#endif
