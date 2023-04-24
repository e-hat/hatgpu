#include "hatpch.h"

#include "application/ForwardRenderer.h"

int main()
{
    hatgpu::ForwardRenderer app("../scenes/monkey-face.json");
    app.Run();

    return 0;
}
