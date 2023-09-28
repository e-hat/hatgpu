#ifndef _INCLUDE_APPLICATION_H
#define _INCLUDE_APPLICATION_H
#include <vulkan/vulkan_core.h>
#include "application/DrawCtx.h"
#include "hatpch.h"
#include "renderers/BdptRenderer.h"
#include "renderers/ForwardRenderer.h"
#include "renderers/layers/AabbLayer.h"

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

namespace hatgpu
{
class Application
{
  public:
    Application(const std::string &windowName, const std::string &scenePath);
    virtual ~Application();

    Application(const Application &other)            = delete;
    Application &operator=(const Application &other) = delete;

    void Init();

    void OnRender();
    void OnImGuiRender();

    void Run();

    void SetFramebufferResized(bool value) { mFramebufferResized = value; }

    static constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;

    // We are setting this to 1 since we will possibly have lots
    // of data on the GPU at once for path tracing. Seem to be easily changed
    // either way -- the path tracer will be implemented agnostic of this.
    static constexpr size_t kMaxFramesInFlight = 1;

  protected:
    LayerStack mLayerStack;
    std::vector<std::shared_ptr<Layer>> mLayers;
    std::shared_ptr<ForwardRenderer> mForwardRenderer;
    std::shared_ptr<BdptRenderer> mBdptRenderer;
    std::shared_ptr<AabbLayer> mAabbLayer;

    LayerRequirements mRequirements;

    void createDepthImage();
    bool checkDeviceExtensionSupport(const VkPhysicalDevice &device);

    void immediateSubmit(std::function<void(VkCommandBuffer)> &&function);

    void PushLayer(std::shared_ptr<Layer> layer)
    {
        layer->OnAttach();
        mLayerStack.PushLayer(std::move(layer));
    }
    void PushOverlay(std::shared_ptr<Layer> layer)
    {
        layer->OnAttach();
        mLayerStack.PushOverlay(std::move(layer));
    }
    void PopLayer(std::shared_ptr<Layer> layer)
    {
        layer->OnDetach();
        mLayerStack.PopLayer(std::move(layer));
    }
    void PopOverlay(std::shared_ptr<Layer> layer)
    {
        layer->OnDetach();
        mLayerStack.PopOverlay(std::move(layer));
    }

    GLFWwindow *mWindow;
    std::string mWindowName;
    InputManager mInputManager;
    Time mTime;

    std::shared_ptr<vk::Ctx> mCtx;
    std::shared_ptr<Scene> mScene;

    uint32_t mCurrentImageIndex;
    VkImage mCurrentSwapchainImage = VK_NULL_HANDLE;
    std::vector<VkImageView> mSwapchainImageViews;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;
    VkImageView mDepthImageView;
    vk::AllocatedImage mDepthImage{};

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
    std::array<DrawCtx, kMaxFramesInFlight> mDrawCtxs{};
    uint32_t mCurrentFrameIndex{0};
    DrawCtx *mCurrentDrawCtx = &mDrawCtxs.front();

    VkQueue mGraphicsQueue;
    uint32_t mGraphicsQueueIndex;
    VkQueue mPresentQueue;

    bool mFramebufferResized{false};

    VkShaderModule createShaderModule(const std::vector<char> &code);
    void recreateSwapchain();

    vk::DeletionQueue mDeleter;

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
};
}  // namespace hatgpu
#endif
