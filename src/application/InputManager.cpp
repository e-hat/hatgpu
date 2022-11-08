#include "application/InputManager.h"

#include <GLFW/glfw3.h>

namespace efvk
{
void InputManager::SetGLFWCallbacks(GLFWwindow *window, Camera *camera)
{
    mCamera = camera;

    glfwSetWindowUserPointer(window, static_cast<void *>(this));
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
}

void InputManager::ProcessInput(GLFWwindow *window, float deltaTime)
{
    processInput(window, deltaTime);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void InputManager::mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    InputManager *manager = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        if (manager->mFirstMouse)
        {
            manager->mLastX      = (float)xpos;
            manager->mLastY      = (float)ypos;
            manager->mFirstMouse = false;
        }

        float xoffset = (float)xpos - manager->mLastX;
        float yoffset =
            manager->mLastY - (float)ypos;  // reversed since y-coordinates go from bottom to top

        manager->mLastX = (float)xpos;
        manager->mLastY = (float)ypos;

        manager->mCamera->ProcessMouseMovement(xoffset, yoffset);
    }
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void InputManager::scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    InputManager *manager = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
    manager->mCamera->ProcessMouseScroll(static_cast<float>(yoffset));
}

// below is from tutorial https://learnopengl.com/
void InputManager::processInput(GLFWwindow *window, float deltaTime)
{
    glfwPollEvents();
    InputManager *manager = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        manager->mCamera->ProcessKeyboard(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        manager->mCamera->ProcessKeyboard(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        manager->mCamera->ProcessKeyboard(CameraMovement::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        manager->mCamera->ProcessKeyboard(CameraMovement::RIGHT, deltaTime);
}
}  // namespace efvk