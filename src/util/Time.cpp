#include "hatpch.h"

#include "Time.h"

#include <GLFW/glfw3.h>

namespace hatgpu
{
float Time::GetDeltaTime()
{
    float currentFrame = (float)glfwGetTime();
    deltaTime          = currentFrame - lastFrame;
    lastFrame          = currentFrame;
    return deltaTime;
}
}  // namespace hatgpu