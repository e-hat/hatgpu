#ifndef _INCLUDE_LAYER_H
#define _INCLUDE_LAYER_H
#include "hatpch.h"

namespace hatgpu
{
class Layer
{
  public:
    Layer(const std::string &debugName) : mDebugName(debugName) {}
    virtual ~Layer() = default;

    virtual void OnAttach() = 0;
    virtual void OnDetach() = 0;
    virtual void OnRender() = 0;
    virtual void OnImGuiRender() {}

  private:
    std::string mDebugName;
};
}  // namespace hatgpu

#endif  //_INCLUDE_LAYER_H
