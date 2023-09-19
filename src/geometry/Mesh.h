#ifndef _INCLUDE_MESH_H
#define _INCLUDE_MESH_H
#include "geometry/Aabb.h"
#include "geometry/Hittable.h"
#include "hatpch.h"

#include "texture/Texture.h"
#include "vk/allocator.h"
#include "vk/types.h"
#include "vk/upload_context.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hatgpu
{
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;

        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset   = offsetof(Vertex, position);

        attributeDescriptions[1].binding  = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset   = offsetof(Vertex, normal);

        attributeDescriptions[2].binding  = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset   = offsetof(Vertex, uv);

        return attributeDescriptions;
    }
};

struct Mesh : public Hittable
{
    using IndexType = uint32_t;

    std::vector<Vertex> vertices;
    std::vector<IndexType> indices;
    // Maps texture type to lists of texture paths
    // For example, ALBEDO -> { "texture1.png", "texture2.png" }
    std::unordered_map<TextureType, std::string> textures;
    VkDescriptorSet descriptor;

    vk::AllocatedBuffer vertexBuffer;
    vk::AllocatedBuffer indexBuffer;

    bool loadFromObj(const std::string &filename);

    void upload(vk::Allocator &allocator, vk::UploadContext &context);
    void destroyBuffers(vk::Allocator &allocator);

    Aabb BoundingBox(const glm::mat4 &worldTransform) const override;
};
}  // namespace hatgpu

#endif  //_INCLUDE_MESH_H
