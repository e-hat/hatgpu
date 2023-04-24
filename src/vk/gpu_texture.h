#ifndef _INCLUDED_GPU_TEXTURE_H
#define _INCLUDED_GPU_TEXTURE_H
#include "hatpch.h"

#include "vk/allocator.h"
#include "vk/initializers.h"
#include "vk/types.h"

namespace hatgpu
{
namespace vk
{
struct GpuTexture
{
    AllocatedImage image;
    VkImageView imageView;
    uint32_t mipLevels;

    void destroy(Allocator &allocator) { allocator.destroyImage(image); }
};
}  // namespace vk
}  // namespace hatgpu

#endif
