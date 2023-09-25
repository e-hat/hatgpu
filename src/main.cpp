#include "hatpch.h"
#include "renderers/BdptRenderer.h"
#include "renderers/ForwardRenderer.h"

#include <memory>

int main()
{
    // auto app = std::make_unique<hatgpu::BdptRenderer>();
    auto app = std::make_unique<hatgpu::ForwardRenderer>("../scenes/sponza.json");
    app->Init();
    app->Run();

    return 0;
}
