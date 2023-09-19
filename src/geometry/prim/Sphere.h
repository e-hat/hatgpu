#ifndef _INCLUDE_SPHERE_H
#define _INCLUDE_SPHERE_H
#pragma once
#include "hatpch.h"

#include "geometry/Mesh.h"

#include <map>

namespace hatgpu
{
class Sphere : public Mesh
{
  public:
    Sphere(std::size_t vSegments, std::size_t hSegments);

    // Aabb BoundingBox(const glm::mat4 &worldTransform) const override;
};
}  // namespace hatgpu

#endif
