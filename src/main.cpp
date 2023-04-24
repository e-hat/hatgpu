#include "application/BdptRenderer.h"
#include "hatpch.h"

int main()
{
    hatgpu::BdptRenderer app("../scenes/monkey-face.json");
    app.Run();

    return 0;
}
