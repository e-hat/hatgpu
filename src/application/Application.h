#ifndef _INCLUDE_APPLICATION_H
#define _INCLUDE_APPLICATION_H
#include <vulkan/vulkan_core.h>
#include "hatpch.h"

#include "Layer.h"
#include "application/InputManager.h"
#include "util/Time.h"
#include "vk/allocator.h"
#include "vk/ctx.h"
#include "vk/deleter.h"
#include "vk/upload_context.h"

#include <tracy/TracyVulkan.hpp>

#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace hatgpu
{
class Application
{
  public:
    Application(const std::string &windowName);
    virtual ~Application();

    Application(const Application &other)            = delete;
    Application &operator=(const Application &other) = delete;

    virtual void Init();
    virtual void Exit() = 0;

    virtual void OnRender()      = 0;
    virtual void OnImGuiRender() = 0;

    void Run();

    void SetFramebufferResized(bool value) { mFramebufferResized = value; }

    static constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;

  protected:
    std::vector<std::unique_ptr<Layer>> mLayers;

    vk::Allocator mAllocator;

    VkImageView mDepthImageView;
    vk::AllocatedImage mDepthImage{};

    virtual void createDepthImage();
    virtual bool checkDeviceExtensionSupport(const VkPhysicalDevice &device);
    virtual VkImageUsageFlags swapchainImageUsage() const = 0;

    inline constexpr virtual VkSubpassDependency renderSubpassDependency() const
    {

        VkSubpassDependency dependency{};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        return dependency;
    }

    void immediateSubmit(std::function<void(VkCommandBuffer)> &&function);

    vk::UploadContext mUploadContext;

    // We are setting this to 1 since we will possibly have lots
    // of data on the GPU at once for path tracing. Seem to be easily changed
    // either way -- the path tracer will be implemented agnostic of this.
    static constexpr size_t kMaxFramesInFlight = 1;

    GLFWwindow *mWindow;
    std::string mWindowName;
    InputManager mInputManager;
    Time mTime;
    Camera mCamera;
    std::vector<VkImageView> mSwapchainImageViews;

    VkCtx mCtx;

    uint32_t mCurrentImageIndex;
    VkImage mCurrentSwapchainImage = VK_NULL_HANDLE;
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

  private:
    void initWindow();
    void initVulkan();
    void initImGui();

    void renderImGui(VkCommandBuffer commandBuffer);

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    bool isDeviceSuitable(const VkPhysicalDevice &device, const VkSurfaceKHR &surface);
    void createLogicalDevice();
    void createSwapchain();
    void cleanupSwapchain();
    void createSwapchainImageViews();
    void createSyncObjects();
    void createCommandPool();
    void createCommandBuffers();
    void createTracyContexts();
    void createUploadContext();

    vk::DeletionQueue mDeleter;
};
}  // namespace hatgpu
#endif
