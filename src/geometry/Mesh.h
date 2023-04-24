#ifndef _INCLUDE_MESH_H
#define _INCLUDE_MESH_H
#include "hatpch.h"

#include "vk/types.h"

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

struct Texture
{
    Texture(void *pixels, uint32_t width, uint32_t height)
        : pixels(pixels), width(width), height(height)
    {}
    ~Texture();

    Texture(const Texture &other)            = delete;
    Texture &operator=(const Texture &other) = delete;

    Texture(Texture &&other) noexcept : pixels(other.pixels) { other.pixels = nullptr; }
    Texture &operator=(Texture &&other) noexcept
    {
        pixels       = other.pixels;
        other.pixels = nullptr;
        return *this;
    }

    void *pixels;
    uint32_t width;
    uint32_t height;
};

// Caches and manages CPU texture data
struct TextureManager
{
    void loadTexture(const std::string &file);
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
};

enum class TextureType
{
    ALBEDO,
    METALLIC_ROUGHNESS,
};

struct Mesh
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
};
}  // namespace hatgpu

#endif  //_INCLUDE_MESH_H
