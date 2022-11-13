#ifndef _INCLUDE_EFVK_TYPES_H
#define _INCLUDE_EFVK_TYPES_H
#include "efpch.h"

#include <vk_mem_alloc.h>

namespace efvk
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
}  // namespace efvk

#endif  //_INCLUDE_EFVK_TYPES_H
