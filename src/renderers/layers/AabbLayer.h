#ifndef _INCLUDE_AABBLAYER_H
#define _INCLUDE_AABBLAYER_H
#include "application/DrawCtx.h"
#include "hatpch.h"

#include "application/Layer.h"
#include "vk/ctx.h"

namespace hatgpu
{

class AabbLayer : public Layer
{
  public:
    AabbLayer() : Layer("AabbLayer"), mToggled(false) {}

    void OnAttach(vk::Ctx &ctx) override;
    void OnDetach(vk::Ctx &ctx) override;
    void OnRender(DrawCtx &drawCtx) override;
    void OnImGuiRender() override;

  private:
    bool mToggled;

    VkPipelineLayout mLayout;
    VkPipeline mPipeline;
};

}  // namespace hatgpu

#endif  //_INCLUDE_AABBLAYER_H
