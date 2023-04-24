#include "hatpch.h"

#include "geometry/Texture.h"
#include "vk/gpu_texture.h"
#include "vk/initializers.h"

namespace hatgpu
{
vk::GpuTexture Texture::upload(VkDevice device,
                               vk::Allocator &allocator,
                               vk::UploadContext &context)
{
    // Create staging buffer for image
    VkDeviceSize imageSize            = width * height * 4;
    vk::AllocatedBuffer stagingBuffer = allocator.createBuffer(
        imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    // Write the CPU texture data into the staging buffer
    void *data = allocator.map(stagingBuffer);
    std::memcpy(data, pixels, imageSize);
    allocator.unmap(stagingBuffer);

    // Grab the dimensions of the texture
    VkExtent3D imageExtent{};
    imageExtent.width  = width;
    imageExtent.height = height;
    imageExtent.depth  = 1;

    // Calculate the number of mip levels
    auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    // Create the actual image on the GPU that we'll be using
    VkImageCreateInfo imageInfo =
        vk::imageInfo(VK_FORMAT_R8G8B8A8_SRGB,
                      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                      imageExtent, mipLevels);
    vk::AllocatedImage newImage;
    VmaAllocationCreateInfo imgAllocInfo{};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(allocator.Impl, &imageInfo, &imgAllocInfo, &newImage.image, &newImage.allocation,
                   nullptr);

    // Use GPU commands to transfer the data from the staging buffer
    // and create the mip levels with blits
    context.immediateSubmit([&](VkCommandBuffer cmd) {
        // Make a barrier so that we are ready to write to our destination texture
        VkImageSubresourceRange range;
        range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel   = 0;
        range.levelCount     = mipLevels;
        range.baseArrayLayer = 0;
        range.layerCount     = 1;

        VkImageMemoryBarrier imageBarrierTransfer{};
        imageBarrierTransfer.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrierTransfer.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrierTransfer.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrierTransfer.image            = newImage.image;
        imageBarrierTransfer.subresourceRange = range;
        imageBarrierTransfer.srcAccessMask    = 0;
        imageBarrierTransfer.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;

        // Wait on this barrier so that the texture is ready to be written
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &imageBarrierTransfer);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset      = 0;
        copyRegion.bufferRowLength   = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel       = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount     = 1;
        copyRegion.imageExtent                     = imageExtent;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // The barrier that we'll use for our mip map generation
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image                           = newImage.image;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.subresourceRange.levelCount     = 1;

        int32_t mipWidth  = width;
        int32_t mipHeight = height;
        for (uint32_t i = 1; i < mipLevels; ++i)
        {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
            // We want to transition the previous mip level to a VK_IMAGE_LAYOUT_TRANSFER_SRC
            // layout so that we can use it as source for the blit operation
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0]             = {0, 0, 0};
            blit.srcOffsets[1]             = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // we are sourcing from the last mip level
            blit.srcSubresource.mipLevel       = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount     = 1;
            blit.dstOffsets[0]                 = {0, 0, 0};
            blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1,
                                  mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // our destination is the current mip level
            blit.dstSubresource.mipLevel       = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount     = 1;

            // Perform the blit operation
            vkCmdBlitImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                           VK_FILTER_LINEAR);

            // Now we transfer the old source mip level to a
            // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL layout since we'll be using it in the
            // fragment shader next
            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            // We are doing this as the transfer pipeline stage and we will need this image next
            // in the fragment shader stage
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                                 1, &barrier);

            if (mipWidth > 1)
                mipWidth /= 2;
            if (mipHeight > 1)
                mipHeight /= 2;
        }

        // Now we finally transfer the last mip level to be
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL since that wasn't handled by the loop.
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);
    });

    vk::GpuTexture result;
    result.mipLevels = mipLevels;
    result.image     = newImage;
    // Create the image view

    allocator.destroyBuffer(stagingBuffer);

    // Create the image view
    VkImageViewCreateInfo imageViewInfo = vk::imageViewInfo(VK_FORMAT_R8G8B8A8_SRGB, newImage.image,
                                                            VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
    vkCreateImageView(device, &imageViewInfo, nullptr, &result.imageView);

    return result;
}
}  // namespace hatgpu
