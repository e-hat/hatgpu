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
    Allocator(VmaAllocatorCreateInfo create, DeletionQueue &deleter);

    AllocatedBuffer createBuffer(size_t allocSize,
                                 VkBufferUsageFlags usage,
                                 VmaMemoryUsage memoryUsage);

    void destroy() { vmaDestroyAllocator(Impl); }

    void *map(const AllocatedBuffer &buf);
    void unmap(const AllocatedBuffer &buf);
    void destroyBuffer(const AllocatedBuffer &buf);

    VmaAllocator Impl = VK_NULL_HANDLE;
};
}  // namespace vk
}  // namespace hatgpu

#endif
