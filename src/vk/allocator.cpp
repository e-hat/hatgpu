#include "hatpch.h"

#include "vk/allocator.h"

#include "vk/deleter.h"
#include "vk/types.h"

namespace hatgpu
{
namespace vk
{
Allocator::Allocator(VmaAllocatorCreateInfo create)
{
    H_CHECK(vmaCreateAllocator(&create, &Impl), "Failed to create allocator");
}

AllocatedBuffer Allocator::createBuffer(size_t allocSize,
                                        VkBufferUsageFlags usage,
                                        VmaMemoryUsage memoryUsage)
{
    // allocate vertex buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;

    bufferInfo.size  = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage                   = memoryUsage;

    vk::AllocatedBuffer newBuffer;

    // allocate the buffer
    H_CHECK(vmaCreateBuffer(Impl, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer,
                            &newBuffer.allocation, nullptr),
            "Failed to allocate new buffer of size " + std::to_string(allocSize));

    return newBuffer;
}

void *Allocator::map(const AllocatedBuffer &buf)
{
    void *result;
    vmaMapMemory(Impl, buf.allocation, &result);
    return static_cast<char *>(result);
}

void Allocator::unmap(const AllocatedBuffer &buf)
{
    vmaUnmapMemory(Impl, buf.allocation);
}

void Allocator::destroyBuffer(const AllocatedBuffer &buf)
{
    vmaDestroyBuffer(Impl, buf.buffer, buf.allocation);
}

void Allocator::destroyImage(const AllocatedImage &img)
{
    vmaDestroyImage(Impl, img.image, img.allocation);
}
}  // namespace vk
}  // namespace hatgpu
