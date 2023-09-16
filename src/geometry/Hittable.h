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
    virtual Aabb BoundingBox() const { return {glm::vec4(0), glm::vec4(0)}; }
};
};  // namespace hatgpu

#endif
