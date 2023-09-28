#include "application/Application.h"
#include "hatpch.h"

#include <memory>

int main()
{
    auto app = std::make_unique<hatgpu::Application>("HatGPU", "../scenes/sponza.json");
    app->Init();
    app->Run();

    return 0;
}
