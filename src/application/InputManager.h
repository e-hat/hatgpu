#ifndef _INCLUDE_INPUT_MANAGER_H
#define _INCLUDE_INPUT_MANAGER_H

#include "scene/Camera.h"

class GLFWwindow;

namespace efvk
{
class InputManager
{
  public:
    InputManager() = default;

    void SetGLFWCallbacks(GLFWwindow *window, Camera *camera);
    void ProcessInput(GLFWwindow *window, float deltaTime);

  private:
    static void mouse_callback(GLFWwindow *window, double xpos, double ypos);
    static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
    static void processInput(GLFWwindow *window, float deltaTime);

    bool mFirstMouse = 0;
    float mLastX     = 0;
    float mLastY     = 0;

    Camera *mCamera = nullptr;
};
}  // namespace efvk
#endif