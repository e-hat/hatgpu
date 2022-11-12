#ifndef _INCLUDE_APPLICATION_H
#define _INCLUDE_APPLICATION_H

#include "application/InputManager.h"
#include "util/Time.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <optional>
#include <string>
#include <vector>

namespace efvk
{
class Application
{
  public:
    Application(const std::string &windowName);
    ~Application();

    Application(const Application &other)            = delete;
    Application &operator=(const Application &other) = delete;

    virtual void Init() = 0;
    virtual void Exit() = 0;

    virtual void OnRender()      = 0;
    virtual void OnImGuiRender() = 0;

    void Run();

  protected:
    GLFWwindow *mWindow;
    InputManager mInputManager;
    Time mTime;
    Camera mCamera;
    std::string mWindowName;
    std::vector<VkImageView> mSwapchainImageViews;
    VkCommandPool mCommandPool;
    VkCommandBuffer mCommandBuffer;
    VkInstance mInstance;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice;
    VkSurfaceKHR mSurface;
    VkSwapchainKHR mSwapchain;
    VkFormat mSwapchainImageFormat;
    VkExtent2D mSwapchainExtent;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;
    void createFramebuffers(const VkRenderPass &renderPass);

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool IsComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };
    static QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice &device,
                                                const VkSurfaceKHR &surface);

    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;
    VkSemaphore mImageAvailable;
    VkSemaphore mRenderFinished;
    VkFence mInFlight;
    uint32_t mCurrentImageIndex;

    VkShaderModule createShaderModule(const std::vector<char> &code);

  private:
    void initWindow();
    void initVulkan();

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    static bool isDeviceSuitable(const VkPhysicalDevice &device, const VkSurfaceKHR &surface);
    void createLogicalDevice();
    void createSwapchain();
    void createSwapchainImageViews();
    void createSyncObjects();
};
}  // namespace efvk
#endif
