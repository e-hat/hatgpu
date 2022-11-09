#ifndef _INCLUDE_HELLOTRIANGLE_H
#define _INCLUDE_HELLOTRIANGLE_H

#include "application/InputManager.h"
#include "util/Time.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <string>

namespace efvk
{
class HelloTriangle
{
  public:
    HelloTriangle(const std::string &windowName);
    ~HelloTriangle();

    HelloTriangle(const HelloTriangle &other)            = delete;
    HelloTriangle &operator=(const HelloTriangle &other) = delete;

    void Init();
    void Exit();

    void OnRender();
    void OnImGuiRender();

    void Run();

  protected:
    GLFWwindow *mWindow;
    InputManager mInputManager;
    Time mTime;
    Camera mCamera;
    std::string mWindowName;

  private:
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool IsComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };
    QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice &device);
    bool isDeviceSuitable(const VkPhysicalDevice &device);

    void initWindow();
    void initVulkan();

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    VkInstance mInstance;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice;
    VkSurfaceKHR mSurface;

    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;
};
}  // namespace efvk
#endif
