#include "application/InputManager.h"
#include "hatpch.h"

#include "application/Application.h"
#include "util/Time.h"

#include <GLFW/glfw3.h>

namespace hatgpu
{
void InputManager::SetGLFWCallbacks(GLFWwindow *window, Camera *camera)
{
    mCamera = camera;

    glfwSetWindowUserPointer(window, static_cast<void *>(this));
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow *window, [[maybe_unused]] int width, [[maybe_unused]] int height) {
            auto manager = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
            manager->mApp->SetFramebufferResized(true);
        });
}

void InputManager::ProcessInput(GLFWwindow *window, float deltaTime)
{
    processInput(window, deltaTime);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void InputManager::mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    auto manager = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
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
void InputManager::scroll_callback(GLFWwindow *window,
                                   [[maybe_unused]] double xoffset,
                                   [[maybe_unused]] double yoffset)
{
    auto manager = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
    manager->mCamera->ProcessMouseScroll(static_cast<float>(yoffset));
}

// below is from tutorial https://learnopengl.com/
void InputManager::processInput(GLFWwindow *window, float deltaTime)
{
    glfwPollEvents();
    auto manager = static_cast<InputManager *>(glfwGetWindowUserPointer(window));
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
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        manager->mCamera->ProcessKeyboard(CameraMovement::LOOKLEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        manager->mCamera->ProcessKeyboard(CameraMovement::LOOKRIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        manager->mCamera->ProcessKeyboard(CameraMovement::LOOKUP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        manager->mCamera->ProcessKeyboard(CameraMovement::LOOKDOWN, deltaTime);
}
}  // namespace hatgpu
