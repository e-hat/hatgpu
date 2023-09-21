#ifndef _INCLUDE_AABBLAYER_H
#define _INCLUDE_AABBLAYER_H
#include "hatpch.h"

#include "application/Layer.h"
#include "vk/ctx.h"

namespace hatgpu
{

class AabbLayer : public Layer
{
  public:
    AabbLayer() : Layer("AabbLayer"), mToggled(false) {}

    void OnAttach(const VkCtx &ctx) override;
    void OnDetach(const VkCtx &ctx) override;
    void OnRender() override;
    void OnImGuiRender() override;

  private:
    bool mToggled;

    VkPipelineLayout mLayout;
    VkPipeline mPipeline;
};

}  // namespace hatgpu

#endif  //_INCLUDE_AABBLAYER_H
