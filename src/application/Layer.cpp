#include "hatpch.h"

#include "Layer.h"
#include "application/Renderer.h"

namespace hatgpu
{
void LayerStack::SetRenderer(std::shared_ptr<Renderer> renderer)
{
    H_ASSERT(renderer != nullptr, "parameter must not be nullptr");

    if (mLayers.empty())
        mLayers.push_back(nullptr);
    mLayers[0] = std::static_pointer_cast<Layer>(renderer);
}

std::optional<std::shared_ptr<Renderer>> LayerStack::renderer()
{
    if (mLayers.empty())
        return std::nullopt;
    if (auto result = std::dynamic_pointer_cast<Renderer>(mLayers.front()); result != nullptr)
        return result;
    return std::nullopt;
}

void LayerStack::PushOverlay(std::shared_ptr<Layer> layer)
{
    mLayers.emplace_back(std::move(layer));
}

void LayerStack::PopOverlay(std::shared_ptr<Layer> layer)
{
    auto it = std::find(begin(), end(), layer);
    if (it != end())
    {
        mLayers.erase(it);
    }
}
}  // namespace hatgpu
