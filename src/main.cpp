#include "hatpch.h"
#include "renderers/BdptRenderer.h"

#include <memory>

int main()
{
    auto app = std::make_unique<hatgpu::BdptRenderer>();
    app->Init();
    app->Run();

    return 0;
}
