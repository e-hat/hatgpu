#ifndef _INCLUDE_MODEL_H
#define _INCLUDE_MODEL_H

#include "geometry/Mesh.h"

#include <memory>
#include <string>
#include <vector>

class aiNode;
class aiScene;

namespace efvk
{

struct Model
{
    void loadFromObj(const std::string &filename);

    void processNode(aiNode *node, const aiScene *scene);

    std::vector<Mesh> meshes;
};
}  // namespace efvk

#endif  //_INCLUDE_MODEL_H
