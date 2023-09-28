#ifndef _INCLUDE_SCENE_H
#define _INCLUDE_SCENE_H
#include "hatpch.h"

#include "geometry/Model.h"
#include "scene/Camera.h"
#include "texture/Texture.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace hatgpu
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

    Camera camera;
    TextureManager textureManager;

    void loadFromJson(const std::string &path);
};
}  // namespace hatgpu

#endif  //_INCLUDE_SCENE_H
