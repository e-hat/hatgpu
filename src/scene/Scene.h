#ifndef _INCLUDE_SCENE_H
#define _INCLUDE_SCENE_H

#include "geometry/Model.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace efvk
{
struct RenderObject
{
    std::shared_ptr<Model> model;
    glm::mat4 transform;
};

struct PointLight
{
    glm::vec3 position;
    glm::vec3 color = glm::vec3(1.f);
};

struct DirLight
{
    glm::vec3 color;
    glm::vec3 direction;
};

struct Scene
{
    std::vector<RenderObject> renderables;
    std::vector<PointLight> pointLights;
    DirLight dirLight;

    void loadFromJson(const std::string &path, TextureManager &textureManager);
};
}  // namespace efvk

#endif  //_INCLUDE_SCENE_H
