#ifndef _INCLUDE_AABBLAYER_H
#define _INCLUDE_AABBLAYER_H
#include "hatpch.h"

#include "application/DrawCtx.h"
#include "scene/Scene.h"

#include "application/Layer.h"
#include "vk/ctx.h"
#include "vk/deleter.h"

namespace hatgpu
{

class AabbLayer : public Layer
{
  public:
    AabbLayer(std::shared_ptr<vk::Ctx> ctx, std::shared_ptr<Scene> scene)
        : Layer("AabbLayer", ctx, scene)
    {}

    void OnAttach() override;
    void OnDetach() override;
    void OnRender(DrawCtx &drawCtx) override;
    void OnImGuiRender() override;

    const static LayerRequirements kRequirements;

  private:
    VkPipelineLayout mLayout;
    VkPipeline mPipeline;
};

}  // namespace hatgpu

#endif  //_INCLUDE_AABBLAYER_H
