#ifndef _INCLUDE_LAYER_H
#define _INCLUDE_LAYER_H
#include "application/DrawCtx.h"
#include "hatpch.h"

#include "vk/ctx.h"
#include "vk/deleter.h"

namespace hatgpu
{
class Layer
{
  public:
    Layer(const std::string &debugName) : mDebugName(debugName) {}

    virtual void OnAttach(vk::Ctx &ctx, vk::DeletionQueue &deleter) = 0;
    virtual void OnDetach(vk::Ctx &ctx)                             = 0;
    virtual void OnRender(DrawCtx &drawCtx)                         = 0;
    virtual void OnImGuiRender() {}

  protected:
    bool mInitialized{false};

  private:
    std::string mDebugName;
};

class LayerStack
{
  public:
    LayerStack() = default;

    // Layers could probably be stored by value here, but I don't want to
    // put that restriction on future Layers since they could be complicated.
    void PushLayer(std::shared_ptr<Layer> layer);
    void PushOverlay(std::shared_ptr<Layer> layer);
    void PopLayer(std::shared_ptr<Layer> layer);
    void PopOverlay(std::shared_ptr<Layer> layer);

    inline std::vector<std::shared_ptr<Layer>>::iterator begin() { return mLayers.begin(); }
    inline std::vector<std::shared_ptr<Layer>>::iterator end() { return mLayers.end(); }

  private:
    std::vector<std::shared_ptr<Layer>> mLayers;
    uint32_t mLayerInsertIndex = 0;
};
}  // namespace hatgpu

#endif  //_INCLUDE_LAYER_H
