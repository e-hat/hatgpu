#ifndef _INCLUDED_TEXTURE_H
#define _INCLUDED_TEXTURE_H
#include "hatpch.h"

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
};
}  // namespace vk
}  // namespace hatgpu

#endif
