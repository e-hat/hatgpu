#ifndef _INCLUDE_QUAD_H
#define _INCLUDE_QUAD_H
#include "hatpch.h"

#include "geometry/Mesh.h"

namespace hatgpu
{

class Quad : public Mesh
{
  public:
    Quad();

    // Aabb BoundingBox(const glm::mat4 &worldTransform) const = 0;
};
}  // namespace hatgpu

#endif
