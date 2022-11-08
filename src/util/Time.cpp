#include "Time.h"

#include <GLFW/glfw3.h>

namespace efvk
{
float Time::GetDeltaTime()
{
    float currentFrame = (float)glfwGetTime();
    deltaTime          = currentFrame - lastFrame;
    lastFrame          = currentFrame;
    return deltaTime;
}
}  // namespace efvk