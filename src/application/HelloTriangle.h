#ifndef _INCLUDE_HELLOTRIANGLE_H
#define _INCLUDE_HELLOTRIANGLE_H

#include "application/InputManager.h"
#include "util/Time.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <string>

namespace efvk
{
class HelloTriangle
{
  public:
    HelloTriangle(const std::string &windowName);
    ~HelloTriangle();

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

  private:
    void initWindow(const std::string &windowName);
    void initVulkan();
    void createInstance();

    VkInstance mInstance;
};
}  // namespace efvk
#endif