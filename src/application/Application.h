#ifndef _INCLUDE_APPLICATION_H
#define _INCLUDE_APPLICATION_H
#include "efpch.h"

#include "application/InputManager.h"
#include "util/Time.h"

#include <tracy/TracyVulkan.hpp>

#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace efvk
{
class Application
{
  public:
    Application(const std::string &windowName);
    virtual ~Application();

    Application(const Application &other)            = delete;
    Application &operator=(const Application &other) = delete;

    virtual void Init() = 0;
    virtual void Exit() = 0;

    virtual void OnRender()            = 0;
    virtual void OnImGuiRender()       = 0;
    virtual void OnRecreateSwapchain() = 0;

    void Run();

    void SetFramebufferResized(bool value) { mFramebufferResized = value; }

  protected:
    static constexpr int kMaxFramesInFlight = 2;

    GLFWwindow *mWindow;
    std::string mWindowName;
    InputManager mInputManager;
    Time mTime;
    Camera mCamera;
    std::vector<VkImageView> mSwapchainImageViews;

    VkInstance mInstance;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties mGpuProperties;
    VkDevice mDevice;
    VkSurfaceKHR mSurface;
    VkSwapchainKHR mSwapchain;
    VkFormat mSwapchainImageFormat;
    VkExtent2D mSwapchainExtent;

    uint32_t mCurrentImageIndex;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> computeFamily;

        bool IsComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value() &&
                   computeFamily.has_value();
        }
    };
    static QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice &device,
                                                const VkSurfaceKHR &surface);

    VkCommandPool mCommandPool;
    struct FrameData
    {
        VkCommandBuffer commandBuffer;
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;
        TracyVkCtx tracyContext;
    };
    std::array<FrameData, kMaxFramesInFlight> mFrames{};
    uint32_t mCurrentFrameIndex{0};
    FrameData *mCurrentApplicationFrame = &mFrames.front();

    VkQueue mGraphicsQueue;
    uint32_t mGraphicsQueueIndex;
    VkQueue mPresentQueue;

    bool mFramebufferResized{false};

    VkShaderModule createShaderModule(const std::vector<char> &code);
    void recreateSwapchain();

    using Deleter = std::function<void()>;

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
    void cleanupSwapchain();
    void createSwapchainImageViews();
    void createSyncObjects();
    void createCommandPool();
    void createCommandBuffers();
    void createTracyContexts();
    std::deque<Deleter> mDeleters;
};
}  // namespace efvk
#endif
