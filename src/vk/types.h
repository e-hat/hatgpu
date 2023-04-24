#ifndef _INCLUDE_TYPES_H
#define _INCLUDE_TYPES_H
#include "hatpch.h"

#include <vk_mem_alloc.h>

namespace hatgpu
{
namespace vk
{
struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct AllocatedImage
{
    VkImage image;
    VmaAllocation allocation;
};

}  // namespace vk
}  // namespace hatgpu

#endif  //_INCLUDE_TYPES_H
