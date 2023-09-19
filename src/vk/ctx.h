#ifndef _INCLUDE_CTX_H
#define _INCLUDE_CTX_H
#include "hatpch.h"

namespace hatgpu
{
struct VkCtx
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
};
}  // namespace hatgpu

#endif
