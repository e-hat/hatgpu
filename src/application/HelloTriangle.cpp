#include "application/HelloTriangle.h"

namespace efvk
{
namespace
{
static constexpr int kWidth  = 1080;
static constexpr int kHeight = 920;
}  // namespace

HelloTriangle::HelloTriangle(const std::string &windowName) : mWindow(nullptr)
{
    initWindow(windowName);
    initVulkan();
}

void HelloTriangle::initWindow(const std::string &windowName)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mWindow = glfwCreateWindow(kWidth, kHeight, windowName.c_str(), nullptr, nullptr);

    mInputManager.SetGLFWCallbacks(mWindow, &mCamera);
}

void HelloTriangle::initVulkan() {}

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

HelloTriangle::~HelloTriangle()
{
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}
}  // namespace efvk