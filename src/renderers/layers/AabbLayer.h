#ifndef _INCLUDE_AABBLAYER_H
#define _INCLUDE_AABBLAYER_H
#include "application/DrawCtx.h"
#include "hatpch.h"

#include "application/Layer.h"
#include "vk/ctx.h"
#include "vk/deleter.h"

namespace hatgpu
{

class AabbLayer : public Layer
{
  public:
    AabbLayer() : Layer("AabbLayer") {}

    void OnAttach(vk::Ctx &ctx, vk::DeletionQueue &deleter) override;
    void OnDetach(vk::Ctx &ctx) override;
    void OnRender(DrawCtx &drawCtx) override;
    void OnImGuiRender() override;

  private:
    VkPipelineLayout mLayout;
    VkPipeline mPipeline;
};

}  // namespace hatgpu

#endif  //_INCLUDE_AABBLAYER_H
