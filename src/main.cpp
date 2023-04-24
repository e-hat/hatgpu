#include "application/ForwardRenderer.h"
#include "hatpch.h"

int main()
{
    hatgpu::ForwardRenderer app("../scenes/monkey-face.json");
    app.Run();

    return 0;
}
