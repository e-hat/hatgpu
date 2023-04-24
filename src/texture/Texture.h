#ifndef _INCLUDED_TEXTURE_H
#define _INCLUDED_TEXTURE_H
#include <vulkan/vulkan_core.h>
#include "hatpch.h"

#include "vk/allocator.h"
#include "vk/gpu_texture.h"
#include "vk/upload_context.h"

namespace hatgpu
{
class Texture
{
  public:
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

    static inline void checkRequiredFormatProperties(VkPhysicalDevice device)
    {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(device, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);

        // Check whether linear GPU blitting (required by mipmaps) is supported
        H_ASSERT(formatProperties.optimalTilingFeatures &
                     VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
                 "Texture image format does not support linear blitting");
    }

    vk::GpuTexture upload(VkDevice device, vk::Allocator &allocator, vk::UploadContext &context);

  private:
    void *pixels;
    uint32_t width;
    uint32_t height;
};

enum class TextureType
{
    ALBEDO,
    METALLIC_ROUGHNESS,
};

// Caches and manages CPU texture data
struct TextureManager
{
    void loadTexture(const std::string &file);
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
};
}  // namespace hatgpu

#endif
