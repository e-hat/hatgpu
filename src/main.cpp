#include "hatpch.h"
#include "renderers/BdptRenderer.h"

int main()
{
    hatgpu::BdptRenderer app("../scenes/monkey-face.json");
    app.Run();

    return 0;
}
