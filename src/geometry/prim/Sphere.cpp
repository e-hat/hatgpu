#include "hatpch.h"

#include "Sphere.h"

namespace hatgpu
{
Sphere::Sphere(std::size_t vSegments, std::size_t hSegments)
{
    float vStep = glm::pi<float>() / (float)vSegments;
    float hStep = 2 * glm::pi<float>() / (float)hSegments;

    for (std::size_t v = 0; v <= vSegments; ++v)
    {
        for (std::size_t h = 0; h <= hSegments; ++h)
        {
            float phi   = glm::pi<float>() / 2.0f - v * vStep;
            float theta = h * hStep;

            float x = glm::cos(phi) * glm::cos(theta);
            float y = glm::cos(phi) * glm::sin(theta);
            float z = glm::sin(phi);

            vertices.push_back(
                Vertex{glm::vec3(x, y, z), glm::vec3(x, y, z),
                       glm::vec2((float)h / (float)hSegments, (float)v / (float)vSegments)});
        }
    }

    int k1, k2;
    for (std::size_t i = 0; i < vSegments; ++i)
    {
        k1 = i * (hSegments + 1);
        k2 = k1 + hSegments + 1;

        for (std::size_t j = 0; j < hSegments; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != vSegments - 1)
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}
}  // namespace hatgpu
