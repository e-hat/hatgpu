#ifndef _INCLUDE_LAYER_H
#define _INCLUDE_LAYER_H
#include "application/DrawCtx.h"
#include "hatpch.h"

#include "vk/ctx.h"

namespace hatgpu
{
class Layer
{
  public:
    Layer(const std::string &debugName) : mDebugName(debugName) {}

    virtual void OnAttach(vk::Ctx &ctx)     = 0;
    virtual void OnDetach(vk::Ctx &ctx)     = 0;
    virtual void OnRender(DrawCtx &drawCtx) = 0;
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

    void PushLayer(std::unique_ptr<Layer> layer);
    void PushOverlay(std::unique_ptr<Layer> layer);
    void PopLayer(std::unique_ptr<Layer> layer);
    void PopOverlay(std::unique_ptr<Layer> layer);

    inline std::vector<std::unique_ptr<Layer>>::iterator begin() { return mLayers.begin(); }
    inline std::vector<std::unique_ptr<Layer>>::iterator end() { return mLayers.end(); }

  private:
    std::vector<std::unique_ptr<Layer>> mLayers;
    uint32_t mLayerInsertIndex = 0;
};
}  // namespace hatgpu

#endif  //_INCLUDE_LAYER_H
