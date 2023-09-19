#ifndef _INCLUDE_HITTABLE_H
#define _INCLUDE_HITTABLE_H
#include "hatpch.h"

#include "Aabb.h"

namespace hatgpu
{
class Hittable
{
  public:
    // TODO: remove this dummy impl
    // Takes a model matrix as parameter
    virtual Aabb BoundingBox([[maybe_unused]] const glm::mat4 &worldTransform) const
    {
        return {glm::vec4(0), glm::vec4(0)};
    }
};
};  // namespace hatgpu

#endif
