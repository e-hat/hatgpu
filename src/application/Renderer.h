#ifndef _INCLUDE_RENDERER_H
#define _INCLUDE_RENDERER_H

#include "application/Layer.h"

namespace hatgpu
{
class Renderer : public Layer
{
  public:
    Renderer(const std::string &debugName,
             std::shared_ptr<vk::Ctx> ctx,
             std::shared_ptr<Scene> scene)
        : Layer(debugName, ctx, scene)
    {}
};
}  // namespace hatgpu

#endif  //_INCLUDE_RENDERER_H