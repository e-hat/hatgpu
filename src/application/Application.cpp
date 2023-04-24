#include "hatpch.h"

#include "application/Application.h"
#include "vk/initializers.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <vector>

namespace hatgpu
{
namespace
{
constexpr int kWidth  = 1366;
constexpr int kHeight = 768;

const std::vector<const char *> kValidationLayers = {"VK_LAYER_KHRONOS_validation"};
constexpr bool kEnableValidationLayers =
#ifdef DEBUG
    true;
#else
    false;
#endif

const std::vector<const char *> kDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

bool checkValidationLayerSupport()
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto validationLayerName : kValidationLayers)
    {
        bool foundLayer = false;
        for (const auto &layerProperties : availableLayers)
        {
            if (std::strcmp(validationLayerName, layerProperties.layerName) == 0)
            {
                foundLayer = true;
                break;
            }
        }

        if (!foundLayer)
        {
            return false;
        }
    }

    return true;
}

std::vector<const char *> getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if constexpr (kEnableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void * /*pUserData*/)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        // use std::endl to flush the stream, else errors might pop up at random times during
        // execution (this actually happened :O)
        std::cerr << "HATGPU: [VULKAN VALIDATION ERROR]: " << pCallbackData->pMessage << std::endl;
#ifdef VALIDATION_DEBUG_BREAK
        __builtin_trap();
#endif
    }

    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

[[maybe_unused]] void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                                    VkDebugUtilsMessengerEXT debugMessenger,
                                                    const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
{
    createInfo                 = {};
    createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

bool checkDeviceExtensionSupport(const VkPhysicalDevice &device)
{
    uint32_t supportedExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount,
                                         supportedExtensions.data());

    std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto &extension : supportedExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};
SwapchainSupportDetails querySwapchainSupport(const VkPhysicalDevice &device,
                                              const VkSurfaceKHR &surface)
{
    SwapchainSupportDetails details{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                              details.presentModes.data());

    return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    for (const auto &format : availableFormats)
    {
        if (format.format == VK_FORMAT_B8G8R8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    for (const auto presentMode : availablePresentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return presentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

[[maybe_unused]] VkSampleCountFlagBits getMaxSampleCount(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts &
                                properties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT)
    {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT)
    {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT)
    {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT)
    {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT)
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT)
    {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}
}  // namespace

Application::Application(const std::string &windowName)
    : mWindow(nullptr), mWindowName(std::move(windowName)), mInputManager(this), mCamera()
{
    mCamera.Position = {0.f, 0.f, 3.f};
    initWindow();
    initVulkan();
    initImGui();
}

void Application::initWindow()
{
    H_LOG("Initializing GLFW window...");
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mWindow = glfwCreateWindow(kWidth, kHeight, mWindowName.c_str(), nullptr, nullptr);
    H_ASSERT(mWindow != nullptr, "Could not create GLFWwindow");

    mInputManager.SetGLFWCallbacks(mWindow, &mCamera);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying GLFW window");
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    });
}

void Application::initVulkan()
{
    H_LOG("Initializing vulkan...");
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createSwapchainImageViews();
    mDeleter.enqueue([this]() { cleanupSwapchain(); });
    createSyncObjects();
    createCommandPool();
    createCommandBuffers();
    createTracyContexts();
    createUploadContext();
}

void Application::initImGui()
{
    H_LOG("Initializing Dear ImGui");
    IMGUI_CHECKVERSION();

    createUiPass();
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets                    = 1000;
    poolInfo.poolSizeCount              = std::size(poolSizes);
    poolInfo.pPoolSizes                 = poolSizes;

    VkDescriptorPool imguiPool;
    H_CHECK(vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &imguiPool),
            "Failed to create descriptor pool for Dear ImGui");

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // this initializes imgui for SDL
    ImGui_ImplGlfw_InitForVulkan(mWindow, true);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance                  = mInstance;
    initInfo.PhysicalDevice            = mPhysicalDevice;
    initInfo.Device                    = mDevice;
    initInfo.Queue                     = mGraphicsQueue;
    initInfo.DescriptorPool            = imguiPool;
    initInfo.MinImageCount             = 3;
    initInfo.ImageCount                = 3;
    initInfo.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo, mUiPass);

    // execute a gpu command to upload imgui font textures
    immediateSubmit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

    // clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    // add the destroy the imgui created structures
    mDeleter.enqueue([imguiPool, this] {
        H_LOG("...destroying Dear ImGui descriptor pool");
        vkDestroyDescriptorPool(mDevice, imguiPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
    });
}

void Application::immediateSubmit(std::function<void(VkCommandBuffer)> &&function)
{
    VkCommandBuffer cmd = mUploadContext.commandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo =
        vk::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    H_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo), "Failed to begin command buffer");

    function(cmd);

    H_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");

    VkSubmitInfo submitInfo = vk::submitInfo(&cmd);
    H_CHECK(vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mUploadContext.uploadFence),
            "Failed to submit to queue");

    vkWaitForFences(mDevice, 1, &mUploadContext.uploadFence, true,
                    std::numeric_limits<uint32_t>::max());
    vkResetFences(mDevice, 1, &mUploadContext.uploadFence);

    vkResetCommandPool(mDevice, mUploadContext.commandPool, 0);
}

void Application::createUploadContext()
{
    H_LOG("...creating upload context");
    VkFenceCreateInfo uploadFenceCreateInfo = vk::fenceInfo();
    H_CHECK(vkCreateFence(mDevice, &uploadFenceCreateInfo, nullptr, &mUploadContext.uploadFence),
            "Failed to create upload context fence");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying upload context fence");
        vkDestroyFence(mDevice, mUploadContext.uploadFence, nullptr);
    });

    VkCommandPoolCreateInfo commandPoolCreateInfo = vk::commandPoolInfo(mGraphicsQueueIndex);
    H_CHECK(
        vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr, &mUploadContext.commandPool),
        "Failed to create upload context command pool");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying upload context command pool");
        vkDestroyCommandPool(mDevice, mUploadContext.commandPool, nullptr);
    });

    VkCommandBufferAllocateInfo commandBufferAllocInfo =
        vk::commandBufferAllocInfo(mUploadContext.commandPool);
    H_CHECK(
        vkAllocateCommandBuffers(mDevice, &commandBufferAllocInfo, &mUploadContext.commandBuffer),
        "Failed to allocate upload context command buffer");
}

void Application::createUiPass()
{
    H_LOG("...creating UI render pass");
    // Initialize color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = mSwapchainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Initialize depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = kDepthFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depthDependency{};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.dstSubpass = 0;
    depthDependency.srcStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = 0;
    depthDependency.dstStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    std::array<VkSubpassDependency, 2> dependencies    = {dependency, depthDependency};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies   = dependencies.data();

    H_CHECK(vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mUiPass),
            "Failed to create render pass");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying UI render pass");
        vkDestroyRenderPass(mDevice, mUiPass, nullptr);
    });
}

void Application::Run()
{
    H_LOG("Running application...");
    Init();

    while (!glfwWindowShouldClose(mWindow))
    {
        mInputManager.ProcessInput(mWindow, mTime.GetDeltaTime());
        ZoneScopedC(tracy::Color::Aqua);
        {
            ZoneScopedNC("vkWaitForFences", tracy::Color::Linen);
            vkWaitForFences(mDevice, 1, &mCurrentApplicationFrame->inFlightFence, VK_TRUE,
                            std::numeric_limits<uint64_t>::max());
        }
        {
            ZoneScopedNC("vkAcquireNextImageKHR", tracy::Color::Orchid);
            VkResult nextImageResult =
                vkAcquireNextImageKHR(mDevice, mSwapchain, std::numeric_limits<uint64_t>::max(),
                                      mCurrentApplicationFrame->imageAvailableSemaphore,
                                      VK_NULL_HANDLE, &mCurrentImageIndex);
            if (nextImageResult == VK_ERROR_OUT_OF_DATE_KHR)
            {
                recreateSwapchain();
                return;
            }
            H_ASSERT(nextImageResult == VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR,
                     "Failed to acquire swapchain image");
        }
        vkResetFences(mDevice, 1, &mCurrentApplicationFrame->inFlightFence);
        vkResetCommandBuffer(mCurrentApplicationFrame->commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo = vk::commandBufferBeginInfo();
        H_CHECK(vkBeginCommandBuffer(mCurrentApplicationFrame->commandBuffer, &beginInfo),
                "Failed to begin recording command buffer");

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        OnImGuiRender();

        {
            TracyVkZoneC(mCurrentApplicationFrame->tracyContext,
                         mCurrentApplicationFrame->commandBuffer, "ImGui::Render",
                         tracy::Color::Blue);
            ImGui::Render();
        }

        {
            TracyVkZoneC(mCurrentApplicationFrame->tracyContext,
                         mCurrentApplicationFrame->commandBuffer, "OnRender",
                         tracy::Color::MediumAquamarine);

            OnRender();
        }

        VkRenderPassBeginInfo uiPassBegin = vk::renderPassBeginInfo(
            mUiPass, mSwapchainExtent, mSwapchainFramebuffers[mCurrentImageIndex]);
        uiPassBegin.clearValueCount = 0;
        uiPassBegin.pClearValues    = VK_NULL_HANDLE;

        vkCmdBeginRenderPass(mCurrentApplicationFrame->commandBuffer, &uiPassBegin,
                             VK_SUBPASS_CONTENTS_INLINE);

        {
            TracyVkZoneC(mCurrentApplicationFrame->tracyContext,
                         mCurrentApplicationFrame->commandBuffer, "ImGui RenderDrawData",
                         tracy::Color::Blue);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                            mCurrentApplicationFrame->commandBuffer);
        }

        vkCmdEndRenderPass(mCurrentApplicationFrame->commandBuffer);

        TracyVkCollect(mCurrentApplicationFrame->tracyContext,
                       mCurrentApplicationFrame->commandBuffer);
        H_CHECK(vkEndCommandBuffer(mCurrentApplicationFrame->commandBuffer),
                "Failed to end recording of command buffer");

        VkSubmitInfo submitInfo = vk::submitInfo(&mCurrentApplicationFrame->commandBuffer);
        std::array<VkSemaphore, 1> waitSemaphores = {
            mCurrentApplicationFrame->imageAvailableSemaphore};
        std::array<VkPipelineStageFlags, 1> waitStages = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount               = waitSemaphores.size();
        submitInfo.pWaitSemaphores                  = waitSemaphores.data();
        submitInfo.pWaitDstStageMask                = waitStages.data();
        std::array<VkSemaphore, 1> signalSemaphores = {
            mCurrentApplicationFrame->renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores    = signalSemaphores.data();

        H_CHECK(
            vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mCurrentApplicationFrame->inFlightFence),
            "Failed to submit draw command buffer");

        VkPresentInfoKHR presentInfo             = vk::presentInfo();
        presentInfo.waitSemaphoreCount           = 1;
        presentInfo.pWaitSemaphores              = signalSemaphores.data();
        std::array<VkSwapchainKHR, 1> swapchains = {mSwapchain};
        presentInfo.swapchainCount               = 1;
        presentInfo.pSwapchains                  = swapchains.data();
        presentInfo.pImageIndices                = &mCurrentImageIndex;

        VkResult presentResult = vkQueuePresentKHR(mPresentQueue, &presentInfo);

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
            mFramebufferResized)
        {
            SetFramebufferResized(false);
            recreateSwapchain();
        }
        H_ASSERT(presentResult == VK_SUCCESS, "Failed to present swapchain image");

        mCurrentFrameIndex       = (1 + mCurrentFrameIndex) % kMaxFramesInFlight;
        mCurrentApplicationFrame = &mFrames[mCurrentFrameIndex];

        glfwSwapBuffers(mWindow);
        FrameMark;
    }

    vkDeviceWaitIdle(mDevice);
    Exit();
}

void Application::createInstance()
{
    H_LOG("...creating vulkan instance");
    H_ASSERT(!kEnableValidationLayers || checkValidationLayerSupport(),
             "Validation layers were requested, but not available for use");

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = mWindowName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "hatgpu";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if constexpr (kEnableValidationLayers)
    {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT *>(&debugCreateInfo);
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext             = nullptr;
    }

    const auto extensions              = getRequiredExtensions();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    H_CHECK(vkCreateInstance(&createInfo, nullptr, &mInstance), "Failed to create VkInstance");

    mDeleter.enqueue([this] {
        H_LOG("...destroying instance");
        vkDestroyInstance(mInstance, nullptr);
    });

    uint32_t availableExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount,
                                           availableExtensions.data());
}

void Application::setupDebugMessenger()
{
    H_LOG("...setting up debug messenger");
    if constexpr (!kEnableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    H_CHECK(CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger),
            "Failed to setup debug messenger");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying debug messenger");
        DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
    });
}

void Application::createSurface()
{
    H_LOG("...creating window surface");
    H_CHECK(glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface),
            "Failed to create window surface");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying window surface");
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    });
}

Application::QueueFamilyIndices Application::findQueueFamilies(const VkPhysicalDevice &device,
                                                               const VkSurfaceKHR &surface)
{
    QueueFamilyIndices indices{};

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    uint32_t i = 0;
    for (const auto &queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            indices.computeFamily = i;
        }

        if (indices.IsComplete())
        {
            break;
        }

        ++i;
    }

    return indices;
}

bool Application::isDeviceSuitable(const VkPhysicalDevice &device, const VkSurfaceKHR &surface)
{
    const QueueFamilyIndices indices = findQueueFamilies(device, surface);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapchainAdequate = false;
    if (extensionsSupported)
    {
        SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device, surface);
        swapchainAdequate =
            !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.IsComplete() && extensionsSupported && swapchainAdequate &&
           supportedFeatures.samplerAnisotropy;
}

void Application::pickPhysicalDevice()
{
    H_LOG("...picking physical device");
    uint32_t candidateDeviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &candidateDeviceCount, nullptr);
    H_ASSERT(candidateDeviceCount > 0, "Failed to find GPUs with Vulkan support");
    std::vector<VkPhysicalDevice> candidateDevices(candidateDeviceCount);
    vkEnumeratePhysicalDevices(mInstance, &candidateDeviceCount, candidateDevices.data());

    for (const auto &candidate : candidateDevices)
    {
        if (isDeviceSuitable(candidate, mSurface))
        {
            mPhysicalDevice = candidate;
            break;
        }
    }

    H_ASSERT(mPhysicalDevice != VK_NULL_HANDLE, "Failed to find a suitable GPU");

    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mGpuProperties);

    H_LOG(std::string("...selected physical device ") + mGpuProperties.deviceName);
}

void Application::createLogicalDevice()
{
    H_LOG("...creating logical device");
    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice, mSurface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
    std::set<uint32_t> uniqueQueueFamilies = {*indices.graphicsFamily, *indices.presentFamily};

    float queuePriority = 1.0f;
    for (const auto queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos    = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pEnabledFeatures     = &deviceFeatures;

    createInfo.enabledExtensionCount   = static_cast<uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawFeatures;
    shaderDrawFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shaderDrawFeatures.pNext = nullptr;
    shaderDrawFeatures.shaderDrawParameters = VK_TRUE;
    createInfo.pNext                        = &shaderDrawFeatures;

    if constexpr (kEnableValidationLayers)
    {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    H_CHECK(vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice),
            "Unable to create logical device");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying logical device");
        vkDestroyDevice(mDevice, nullptr);
    });

    vkGetDeviceQueue(mDevice, *indices.graphicsFamily, 0, &mGraphicsQueue);
    mGraphicsQueueIndex = *indices.graphicsFamily;
    vkGetDeviceQueue(mDevice, *indices.presentFamily, 0, &mPresentQueue);
}

void Application::createSwapchain()
{
    H_LOG("...creating swapchain");
    SwapchainSupportDetails swapchainSupport = querySwapchainSupport(mPhysicalDevice, mSurface);

    const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
    const VkPresentModeKHR presentMode     = chooseSwapPresentMode(swapchainSupport.presentModes);
    const VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities, mWindow);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0)
    {
        imageCount = std::min(swapchainSupport.capabilities.maxImageCount, imageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = mSurface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const QueueFamilyIndices indices           = findQueueFamilies(mPhysicalDevice, mSurface);
    std::array<uint32_t, 2> queueFamilyIndices = {indices.graphicsFamily.value(),
                                                  indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    H_CHECK(vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapchain),
            "Failed to create swapchain");

    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

    mSwapchainImageFormat = surfaceFormat.format;
    mSwapchainExtent      = extent;
}

void Application::createSwapchainImageViews()
{
    H_LOG("...creating swapchain image views");
    mSwapchainImageViews.resize(mSwapchainImages.size());

    for (size_t i = 0; i < mSwapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo createInfo = vk::imageViewInfo(
            mSwapchainImageFormat, mSwapchainImages[i], VK_IMAGE_ASPECT_COLOR_BIT);
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        H_CHECK(vkCreateImageView(mDevice, &createInfo, nullptr, &mSwapchainImageViews[i]),
                "Failed to create swapchain image view");
    }
}

void Application::createCommandPool()
{
    H_LOG("...creating command pool");
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice, mSurface);

    VkCommandPoolCreateInfo poolInfo = vk::commandPoolInfo(
        *queueFamilyIndices.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    H_CHECK(vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool),
            "Failed to create command pool");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying command pool");
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    });
}

void Application::createCommandBuffers()
{
    H_LOG("...creating command buffers");
    std::array<VkCommandBuffer, kMaxFramesInFlight> commandBuffers{};

    VkCommandBufferAllocateInfo allocInfo =
        vk::commandBufferAllocInfo(mCommandPool, static_cast<uint32_t>(commandBuffers.size()));
    H_CHECK(vkAllocateCommandBuffers(mDevice, &allocInfo, commandBuffers.data()),
            "Failed to allocate command buffer");

    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        mFrames[i].commandBuffer = commandBuffers[i];
    }
}

void Application::createTracyContexts()
{
    H_LOG("...creating Tracy contexts");
    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        mFrames[i].tracyContext =
            TracyVkContext(mPhysicalDevice, mDevice, mGraphicsQueue, mFrames[i].commandBuffer);
    }

    mDeleter.enqueue([this]() {
        H_LOG("...destroying Tracy contexts");
        for (size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            TracyVkDestroy(mFrames[i].tracyContext);
        }
    });
}

void Application::createSyncObjects()
{
    H_LOG("...creating sync objects");
    VkSemaphoreCreateInfo semaphoreCreateInfo = vk::semaphoreInfo();
    VkFenceCreateInfo fenceCreateInfo         = vk::fenceInfo(VK_FENCE_CREATE_SIGNALED_BIT);

    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        H_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr,
                                  &mFrames[i].imageAvailableSemaphore),
                "Failed to create sync object");
        H_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr,
                                  &mFrames[i].renderFinishedSemaphore),
                "Failed to create sync object");
        H_CHECK(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mFrames[i].inFlightFence),
                "Failed to create sync object");
    }

    mDeleter.enqueue([this]() {
        H_LOG("...destroying frame sync objects");
        for (size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            vkDestroySemaphore(mDevice, mFrames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(mDevice, mFrames[i].renderFinishedSemaphore, nullptr);
            vkDestroyFence(mDevice, mFrames[i].inFlightFence, nullptr);
        }
    });
}

void Application::cleanupSwapchain()
{
    H_LOG("...destroying swapchain");
    for (const auto framebuffer : mSwapchainFramebuffers)
    {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }

    for (const auto imageView : mSwapchainImageViews)
    {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }

    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
}

void Application::recreateSwapchain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(mWindow, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(mWindow, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(mDevice);

    cleanupSwapchain();

    createSwapchain();
    createSwapchainImageViews();
    OnRecreateSwapchain();
}

Application::~Application()
{
    H_LOG("Destroying Application...");
    H_LOG("...waiting for device to be idle");
    vkDeviceWaitIdle(mDevice);

    mDeleter.flush();
}
}  // namespace hatgpu
