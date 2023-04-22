#include "hatpch.h"

#include "application/HatGpuRenderer.h"

int main()
{
    hatgpu::HatGpuRenderer app("../scenes/sponza.json");
    app.Run();

    return 0;
}
