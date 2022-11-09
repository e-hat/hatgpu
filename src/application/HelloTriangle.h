#ifndef _INCLUDE_HELLOTRIANGLE_H
#define _INCLUDE_HELLOTRIANGLE_H

#include "application/InputManager.h"
#include "util/Time.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
    void initWindow();
    void initVulkan();

    void createInstance();
    void setupDebugMessenger();
    void pickPhysicalDevice();

    VkInstance mInstance;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger;
};
}  // namespace efvk
#endif
