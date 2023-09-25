#include "hatpch.h"

#include "Layer.h"

namespace hatgpu
{
void LayerStack::PushLayer(std::shared_ptr<Layer> layer)
{
    mLayers.emplace(begin() + mLayerInsertIndex, std::move(layer));
    ++mLayerInsertIndex;
}

void LayerStack::PushOverlay(std::shared_ptr<Layer> layer)
{
    mLayers.emplace_back(std::move(layer));
}

void LayerStack::PopLayer(std::shared_ptr<Layer> layer)
{
    auto it = std::find(begin(), end(), layer);
    if (it != end())
    {
        mLayers.erase(it);
        --mLayerInsertIndex;
    }
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
