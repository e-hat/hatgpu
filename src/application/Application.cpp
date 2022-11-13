#include "efpch.h"

#include "application/Application.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <vector>

namespace efvk
{
namespace
{
constexpr int kWidth  = 1080;
constexpr int kHeight = 920;

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
        std::cerr << "Validation layer: " << pCallbackData->pMessage << '\n';
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

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
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

}  // namespace

Application::Application(const std::string &windowName)
    : mWindow(nullptr), mWindowName(std::move(windowName)), mInputManager(this), mCamera()
{
    mCamera.Position = {0.f, 0.f, 3.f};
    initWindow();
    initVulkan();
}

void Application::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mWindow = glfwCreateWindow(kWidth, kHeight, mWindowName.c_str(), nullptr, nullptr);
    if (mWindow == nullptr)
    {
        throw std::runtime_error("Could not create GLFWwindow");
    }

    mInputManager.SetGLFWCallbacks(mWindow, &mCamera);

    mDeleters.emplace_back([this]() {
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    });
}

void Application::initVulkan()
{
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createSwapchainImageViews();
    mDeleters.emplace_back([this]() { cleanupSwapchain(); });
    createSyncObjects();
    createCommandPool();
    createCommandBuffers();
}

void Application::Run()
{
    Init();

    while (!glfwWindowShouldClose(mWindow))
    {
        mInputManager.ProcessInput(mWindow, mTime.GetDeltaTime());

        OnRender();

        mCurrentFrameIndex = (1 + mCurrentFrameIndex) % kMaxFramesInFlight;

        OnImGuiRender();

        glfwSwapBuffers(mWindow);
    }

    vkDeviceWaitIdle(mDevice);
    Exit();
}

void Application::createInstance()
{
    if (kEnableValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("Validation layers were requested, but not available for use");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = mWindowName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "efvk";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

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

    if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VkInstance");
    }

    mDeleters.emplace_back([this] { vkDestroyInstance(mInstance, nullptr); });

    uint32_t availableExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount,
                                           availableExtensions.data());
    std::cout << "Available extensions:\n";
    for (const auto &extension : availableExtensions)
    {
        std::cout << '\t' << extension.extensionName << '\n';
    }
}

void Application::setupDebugMessenger()
{
    if constexpr (!kEnableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to setup debug messenger");
    }

    mDeleters.emplace_back(
        [this]() { DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr); });
}

void Application::createSurface()
{
    if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface");
    }

    mDeleters.emplace_back([this]() { vkDestroySurfaceKHR(mInstance, mSurface, nullptr); });
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
        // TODO: query the compute queue family index.

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

    return indices.IsComplete() && extensionsSupported && swapchainAdequate;
}

void Application::pickPhysicalDevice()
{
    uint32_t candidateDeviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &candidateDeviceCount, nullptr);
    if (candidateDeviceCount == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }
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

    if (mPhysicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to find a suitable GPU");
    }
}

void Application::createLogicalDevice()
{
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

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos    = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pEnabledFeatures     = &deviceFeatures;

    createInfo.enabledExtensionCount   = static_cast<uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    if constexpr (kEnableValidationLayers)
    {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS)
    {
        throw std::runtime_error("Unable to create logical device");
    }

    mDeleters.emplace_back([this]() { vkDestroyDevice(mDevice, nullptr); });

    vkGetDeviceQueue(mDevice, *indices.graphicsFamily, 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, *indices.presentFamily, 0, &mPresentQueue);
}

void Application::createSwapchain()
{
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

    if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

    mSwapchainImageFormat = surfaceFormat.format;
    mSwapchainExtent      = extent;
}

void Application::createSwapchainImageViews()
{
    mSwapchainImageViews.resize(mSwapchainImages.size());

    for (size_t i = 0; i < mSwapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image    = mSwapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format   = mSwapchainImageFormat;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(mDevice, &createInfo, nullptr, &mSwapchainImageViews[i]) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swapchain image view");
        }
    }
}

VkShaderModule Application::createShaderModule(const std::vector<char> &code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

void Application::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice, mSurface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = *queueFamilyIndices.graphicsFamily;

    if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool");
    }

    mDeleters.emplace_back([this]() { vkDestroyCommandPool(mDevice, mCommandPool, nullptr); });
}

void Application::createCommandBuffers()
{
    mCommandBuffers.resize(kMaxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = mCommandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());

    if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffer");
    }
}

void Application::createSyncObjects()
{
    mImageAvailableSemaphores.resize(kMaxFramesInFlight);
    mRenderFinishedSemaphores.resize(kMaxFramesInFlight);
    mInFlightFences.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr,
                              &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr,
                              &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create sync objects");
        }
    }

    mDeleters.emplace_back([this]() {
        for (size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
            vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
        }
    });
}

void Application::cleanupSwapchain()
{
    for (size_t i = 0; i < mSwapchainFramebuffers.size(); ++i)
    {
        vkDestroyFramebuffer(mDevice, mSwapchainFramebuffers[i], nullptr);
    }

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        vkDestroyImageView(mDevice, mSwapchainImageViews[i], nullptr);
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
    vkDeviceWaitIdle(mDevice);

    while (!mDeleters.empty())
    {
        Deleter nextDeleter = mDeleters.back();
        nextDeleter();
        mDeleters.pop_back();
    }
}
}  // namespace efvk