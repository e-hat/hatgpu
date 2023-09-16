#ifndef _INCLUDE_MODEL_H
#define _INCLUDE_MODEL_H
#include "geometry/Aabb.h"
#include "geometry/Hittable.h"
#include "hatpch.h"

#include "geometry/Mesh.h"

#include <memory>
#include <string>
#include <vector>

class aiNode;
class aiScene;

namespace hatgpu
{

class Model : public Hittable
{
  public:
    void loadFromObj(const std::string &filename, TextureManager &manager);
    std::vector<Mesh> meshes;

    // virtual Aabb BoundingBox() const override;

  private:
    void processNode(aiNode *node, const aiScene *scene, TextureManager &textureManager);
    std::string mDirectory;
};
}  // namespace hatgpu

#endif  //_INCLUDE_MODEL_H
