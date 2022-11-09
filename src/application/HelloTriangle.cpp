#include "application/HelloTriangle.h"

#include <cstring>
#include <iostream>
#include <vector>

namespace efvk
{
namespace
{
static constexpr int kWidth  = 1080;
static constexpr int kHeight = 920;

static const std::vector<const char *> kValidationLayers = {"VK_LAYER_KHRONOS_validation"};
static constexpr bool kEnableValidationLayers =
#ifdef DEBUG
    true;
#else
    false;
#endif

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
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData)
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
}  // namespace

HelloTriangle::HelloTriangle(const std::string &windowName)
    : mWindow(nullptr), mWindowName(std::move(windowName))
{
    initWindow();
    initVulkan();
}

void HelloTriangle::initWindow()
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
}

void HelloTriangle::initVulkan()
{
    createInstance();
    setupDebugMessenger();
}

void HelloTriangle::Init() {}
void HelloTriangle::Exit() {}

void HelloTriangle::OnRender() {}
void HelloTriangle::OnImGuiRender() {}

void HelloTriangle::Run()
{
    Init();

    while (!glfwWindowShouldClose(mWindow))
    {
        mInputManager.ProcessInput(mWindow, mTime.GetDeltaTime());
        OnRender();
        OnImGuiRender();

        glfwSwapBuffers(mWindow);
    }

    Exit();
}

void HelloTriangle::createInstance()
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

void HelloTriangle::setupDebugMessenger()
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
}

HelloTriangle::~HelloTriangle()
{
    if constexpr (kEnableValidationLayers)
    {
        // DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
    }

    vkDestroyInstance(mInstance, nullptr);

    glfwDestroyWindow(mWindow);
    glfwTerminate();
}
}  // namespace efvk