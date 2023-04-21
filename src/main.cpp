#include "efpch.h"

#include "application/HatGpuRenderer.h"

int main()
{
    hatgpu::HatGpuRenderer app("../scenes/clustered_demo.json");
    app.Run();

    return 0;
}
