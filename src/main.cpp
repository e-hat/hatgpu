#include "hatpch.h"

#include "application/ForwardRenderer.h"

int main()
{
    hatgpu::ForwardRenderer app("../scenes/sponza.json");
    app.Run();

    return 0;
}
