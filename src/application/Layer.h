#ifndef _INCLUDE_LAYER_H
#define _INCLUDE_LAYER_H
#include "hatpch.h"

#include "application/DrawCtx.h"
#include "scene/Scene.h"

#include "vk/allocator.h"
#include "vk/ctx.h"
#include "vk/deleter.h"

namespace hatgpu
{
class Layer
{
  public:
    Layer(const std::string &debugName, std::shared_ptr<vk::Ctx> ctx, std::shared_ptr<Scene> scene)
        : mInitialized(false), mCtx(ctx), mScene(scene), mDebugName(debugName)
    {}

    virtual void OnAttach()                 = 0;
    virtual void OnDetach()                 = 0;
    virtual void OnRender(DrawCtx &drawCtx) = 0;
    virtual void OnImGuiRender() {}

    inline void FlushDeletionQueue()
    {
        H_LOG(std::format("Destroying layer {}", mDebugName.c_str()));
        mDeleter.flush();
    }

    inline std::string DebugName() const { return mDebugName; }

  protected:
    bool mInitialized{false};
    vk::DeletionQueue mDeleter;
    std::shared_ptr<vk::Ctx> mCtx;
    std::shared_ptr<Scene> mScene;

  private:
    std::string mDebugName;
};

struct LayerRequirements
{
    VkImageUsageFlags swapchainImageUsage;
    std::unordered_set<std::string> deviceExtensions;

    LayerRequirements concat(LayerRequirements other) const
    {
        std::unordered_set<std::string> newDeviceExtensions = deviceExtensions;
        for (const auto &extension : other.deviceExtensions)
        {
            newDeviceExtensions.insert(extension);
        }

        return {swapchainImageUsage | other.swapchainImageUsage, newDeviceExtensions};
    }
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
