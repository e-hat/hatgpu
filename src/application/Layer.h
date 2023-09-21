#ifndef _INCLUDE_LAYER_H
#define _INCLUDE_LAYER_H
#include "hatpch.h"

#include "vk/ctx.h"

namespace hatgpu
{
class Layer
{
  public:
    Layer(const std::string &debugName) : mDebugName(debugName) {}
    virtual ~Layer() = default;

    virtual void OnAttach(const VkCtx &ctx) = 0;
    virtual void OnDetach(const VkCtx &ctx) = 0;
    virtual void OnRender()                 = 0;
    virtual void OnImGuiRender() {}

  private:
    std::string mDebugName;
};
}  // namespace hatgpu

#endif  //_INCLUDE_LAYER_H
