#ifndef _INCLUDE_BOX_H
#define _INCLUDE_BOX_H
#include "hatpch.h"

#include "geometry/Mesh.h"

namespace hatgpu
{

class Box : public Mesh
{
  public:
    Box();

    // Aabb BoundingBox(const glm::mat4 &worldTransform) const override;
};
}  // namespace hatgpu

#endif
