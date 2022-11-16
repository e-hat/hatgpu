#ifndef _INCLUDE_MODEL_H
#define _INCLUDE_MODEL_H
#include "efpch.h"

#include "geometry/Mesh.h"

#include <memory>
#include <string>
#include <vector>

class aiNode;
class aiScene;

namespace efvk
{

class Model
{
  public:
    void loadFromObj(const std::string &filename, TextureManager &manager);
    std::vector<Mesh> meshes;

  private:
    void processNode(aiNode *node, const aiScene *scene, TextureManager &textureManager);
};
}  // namespace efvk

#endif  //_INCLUDE_MODEL_H
