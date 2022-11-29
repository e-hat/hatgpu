#include "efpch.h"

#include "application/EfvkRenderer.h"

int main()
{
    efvk::EfvkRenderer app("../scenes/clustered_demo.json");
    app.Run();

    return 0;
}
