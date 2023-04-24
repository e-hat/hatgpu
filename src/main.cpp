#include "application/Application.h"
#include "hatpch.h"
#include "renderers/ForwardRenderer.h"

int main()
{
    hatgpu::ForwardRenderer app("../scenes/monkey-face.json");
    app.Run();

    return 0;
}
