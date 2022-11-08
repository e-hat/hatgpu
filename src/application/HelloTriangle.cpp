#include "application/HelloTriangle.h"

#include <iostream>
#include <vector>

namespace efvk
{
namespace
{
static constexpr int kWidth  = 1080;
static constexpr int kHeight = 920;
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

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount   = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount       = 0;

    if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VkInstance");
    }

    std::cout << "Created the instance successfully!\n";
}

HelloTriangle::~HelloTriangle()
{
    vkDestroyInstance(mInstance, nullptr);

    glfwDestroyWindow(mWindow);
    glfwTerminate();
}
}  // namespace efvk