#ifndef _INCLUDED_ALLOCATOR_H
#define _INCLUDED_ALLOCATOR_H
#include "hatpch.h"

#include "vk/deleter.h"
#include "vk/types.h"

namespace hatgpu
{
namespace vk
{
struct Allocator
{
    Allocator() = default;
    Allocator(VmaAllocatorCreateInfo create);

    AllocatedBuffer createBuffer(size_t allocSize,
                                 VkBufferUsageFlags usage,
                                 VmaMemoryUsage memoryUsage);

    void destroy() { vmaDestroyAllocator(Impl); }

    void *map(const AllocatedBuffer &buf);
    void unmap(const AllocatedBuffer &buf);

    void destroyBuffer(const AllocatedBuffer &buf);
    void destroyImage(const AllocatedImage &img);

    VmaAllocator Impl = VK_NULL_HANDLE;
};
}  // namespace vk
}  // namespace hatgpu

#endif
