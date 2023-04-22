#include "hatpch.h"

#include "Scene.h"
#include "util/Random.h"

#include <nlohmann/json.hpp>

#include <fstream>

using namespace nlohmann;

namespace glm
{
void from_json(const json &j, ::glm::vec3 &v)
{
    H_ASSERT(j.is_array(), "JSON Parse error: expected array type for vec3");

    v.x = j.at(0).get<float>();
    v.y = j.at(1).get<float>();
    v.z = j.at(2).get<float>();
}
}  // namespace glm

namespace hatgpu
{
void from_json(const json &j, PointLight &p)
{
    j.at("position").get_to(p.position);
    j.at("color").get_to(p.color);
}

void from_json(const json &j, DirLight &d)
{
    j.at("direction").get_to(d.direction);
    j.at("color").get_to(d.color);
}

void Scene::loadFromJson(const std::string &path, hatgpu::TextureManager &textureManager)
{
    std::ifstream inputFile(path);
    H_ASSERT(inputFile.is_open(), "Failed to load scene from JSON file");

    json j = json::parse(inputFile);

    if (j.contains("models"))
    {
        for (auto &modelJson : j.at("models"))
        {
            std::string modelPath = modelJson.at("path").get<std::string>();

            json transformJson = modelJson.at("transform");

            glm::mat4 scale       = glm::mat4(1.f);
            glm::mat4 rotation    = glm::mat4(1.f);
            glm::mat4 translation = glm::mat4(1.f);

            if (transformJson.contains("scale"))
            {
                scale = glm::scale(glm::mat4(1.f), transformJson.at("scale").get<glm::vec3>());
            }

            if (transformJson.contains("rotation"))
            {
                json rotationJson = transformJson.at("rotation");
                rotation = glm::rotate(glm::mat4(1.f), rotationJson.at("degrees").get<float>(),
                                       rotationJson.at("axis").get<glm::vec3>());
            }

            if (transformJson.contains("translation"))
            {
                translation = glm::translate(glm::mat4(1.f),
                                             transformJson.at("translation").get<glm::vec3>());
            }

            glm::mat4 modelTransform = translation * rotation * scale;

            RenderObject renderObj{};
            renderObj.model = std::make_shared<Model>();
            renderObj.model->loadFromObj(modelPath, textureManager);
            renderObj.transform = modelTransform;

            renderables.push_back(renderObj);
        };
    }

    if (j.contains("lights"))
    {
        json lightsJson = j.at("lights");
        if (lightsJson.contains("random") && lightsJson.at("random").get<bool>())
        {
            auto rangeMin = lightsJson.at("rangeMin").get<glm::vec3>();
            auto rangeMax = lightsJson.at("rangeMax").get<glm::vec3>();
            auto colors   = lightsJson.at("colors").get<std::vector<glm::vec3>>();
            auto count    = lightsJson.at("count").get<size_t>();

            for (size_t i = 0; i < count; ++i)
            {
                PointLight p;
                p.color    = colors[i % colors.size()];
                p.position = Random::GetRandomInRange<glm::vec3>(rangeMin, rangeMax);
                pointLights.push_back(p);
            }
        }
        else if (lightsJson.contains("pointLights"))
        {
            lightsJson.at("pointLights").get_to(pointLights);
        }

        if (lightsJson.contains("dirLight"))
        {
            lightsJson.at("dirLight").get_to(dirLight);
        }
        else
        {
            dirLight.color = glm::vec3(0.f);
        }
    }
}
}  // namespace hatgpu
