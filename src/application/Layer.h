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

    void OnAttach()
    {
        H_LOG(std::format("Attaching layer '{}'", mDebugName));
        if (!mInitialized)
        {
            Init();
        }
        else
        {
            H_LOG(std::format("...skipping repeated initialization"))
        }

        mInitialized = true;
    }
    virtual void Init()                     = 0;
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
    std::string mDebugName;
};

struct LayerRequirements
{
    VkImageUsageFlags swapchainImageUsage;
    std::unordered_set<std::string> deviceExtensions;
    VkPhysicalDeviceFeatures deviceFeatures{};

    LayerRequirements concat(LayerRequirements other) const
    {
        std::unordered_set<std::string> newDeviceExtensions = deviceExtensions;
        for (const auto &extension : other.deviceExtensions)
        {
            newDeviceExtensions.insert(extension);
        }

        // This is so janky lol
        // when will we have c++ reflection...also why is this struct designed like this
        // im sure there's a good reason
        VkPhysicalDeviceFeatures newDeviceFeatures{};
        for (size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(uint8_t); ++i)
        {
            *(reinterpret_cast<uint8_t *>(&newDeviceFeatures) + i) =
                *(reinterpret_cast<const uint8_t *>(&deviceFeatures) + i) |
                *(reinterpret_cast<const uint8_t *>(&other.deviceFeatures) + i);
        }

        return {swapchainImageUsage | other.swapchainImageUsage, newDeviceExtensions,
                newDeviceFeatures};
    }
};

class Renderer;

class LayerStack
{
  public:
    LayerStack() = default;

    // Layers could probably be stored by value here, but I don't want to
    // put that restriction on future Layers since they could be complicated.
    void SetRenderer(std::shared_ptr<Renderer> renderer);
    void PushOverlay(std::shared_ptr<Layer> layer);
    void PopOverlay(std::shared_ptr<Layer> layer);

    // Either returns nullopt or a non-null shared_ptr<Renderer>
    std::optional<std::shared_ptr<Renderer>> renderer();

    inline std::vector<std::shared_ptr<Layer>>::iterator begin() { return mLayers.begin(); }
    inline std::vector<std::shared_ptr<Layer>>::iterator end() { return mLayers.end(); }

  private:
    std::vector<std::shared_ptr<Layer>> mLayers;
    uint32_t mOverlayInsertIndex = 0;
};
}  // namespace hatgpu

#endif  //_INCLUDE_LAYER_H
