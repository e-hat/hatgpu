#include <vulkan/vulkan_core.h>
#include "hatpch.h"

#include "BdptRenderer.h"
#include "imgui.h"
#include "texture/Texture.h"
#include "util/Random.h"
#include "vk/initializers.h"
#include "vk/shader.h"

#include <glm/gtx/string_cast.hpp>
#include <tracy/Tracy.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
static const std::vector<const char *> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

static constexpr const char *kMainShaderName = "../shaders/bin/bdpt/main.comp.spv";
}  // namespace

namespace hatgpu
{

BdptRenderer::BdptRenderer() : Application("HatGPU") {}

void BdptRenderer::Init()
{
    Application::Init();

    H_LOG("Initializing bdpt renderer...");
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = mPhysicalDevice;
    allocatorInfo.device                 = mDevice;
    allocatorInfo.instance               = mInstance;

    H_LOG("...creating VMA allocator");
    mAllocator = vk::Allocator(allocatorInfo);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying VMA allocator");
        mAllocator.destroy();
    });

    createDescriptorPool();
    createDescriptorLayout();
    createPipeline();
    createCanvas();
    createDescriptorSets();
}

void BdptRenderer::createCanvas()
{
    H_LOG("...creating canvas image");
    for (FrameData &frame : mFrames)
    {
        VkExtent3D imageExtent;
        imageExtent.width           = mSwapchainExtent.width;
        imageExtent.height          = mSwapchainExtent.height;
        imageExtent.depth           = 1;
        VkImageCreateInfo imageInfo = vk::imageInfo(
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            imageExtent);
        imageInfo.flags = VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;

        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM,
                                            &formatProperties);
        H_ASSERT(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
                 "requested image format does not support image storage operations");

        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo imgAllocInfo{};
        frame.canvasImage.mipLevels = 1;
        vk::GpuTexture &img         = frame.canvasImage;
        imgAllocInfo.usage          = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(mAllocator.Impl, &imageInfo, &imgAllocInfo, &img.image.image,
                       &img.image.allocation, nullptr);

        mUploadContext.immediateSubmit([&frame](VkCommandBuffer commandBuffer) {
            VkImageMemoryBarrier barrier;
            barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image               = frame.canvasImage.image.image;
            barrier.pNext               = VK_NULL_HANDLE;

            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;

            barrier.srcAccessMask = VK_ACCESS_NONE;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &barrier);
        });

        VkImageViewCreateInfo canvasViewInfo = vk::imageViewInfo(
            VK_FORMAT_R8G8B8A8_UNORM, frame.canvasImage.image.image, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        H_CHECK(vkCreateImageView(mDevice, &canvasViewInfo, nullptr, &frame.canvasImage.imageView),
                "failed to create canvas image view");
    }

    mDeleter.enqueue([this]() {
        for (FrameData &frame : mFrames)
        {
            vkDestroyImageView(mDevice, frame.canvasImage.imageView, nullptr);
            frame.canvasImage.destroy(mAllocator);
        }
    });
}

bool BdptRenderer::checkDeviceExtensionSupport(const VkPhysicalDevice &device)
{
    uint32_t supportedExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount,
                                         supportedExtensions.data());

    std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto &extension : supportedExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void BdptRenderer::createDescriptorPool()
{
    H_LOG("...creating descriptor set pool");
    std::vector<VkDescriptorPoolSize> sizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
                                               {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 190},
                                               {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
                                               {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10}};

    // Creating the descriptor pool
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = 0;
    poolInfo.maxSets       = 300;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    poolInfo.pPoolSizes    = sizes.data();

    vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying descriptor set pool");
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    });
}

void BdptRenderer::createDescriptorLayout()
{
    H_LOG("...creating main descriptor set layout");

    VkDescriptorSetLayoutBinding canvasBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1);

    VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
    textureLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutInfo.pNext        = nullptr;
    textureLayoutInfo.bindingCount = 1;
    textureLayoutInfo.pBindings    = &canvasBinding;
    textureLayoutInfo.flags        = 0;

    vkCreateDescriptorSetLayout(mDevice, &textureLayoutInfo, nullptr, &mGlobalSetLayout);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying descriptor set layout");
        vkDestroyDescriptorSetLayout(mDevice, mGlobalSetLayout, nullptr);
    });
}

void BdptRenderer::createDescriptorSets()
{
    H_LOG("...creating main descriptor set");

    // Create canvas sampler
    VkSamplerCreateInfo canvasSamplerInfo = vk::samplerInfo(VK_FILTER_LINEAR);
    canvasSamplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    canvasSamplerInfo.maxLod              = 1;
    canvasSamplerInfo.anisotropyEnable    = VK_FALSE;
    canvasSamplerInfo.mipLodBias          = 0.f;
    VkSampler canvasSampler;
    vkCreateSampler(mDevice, &canvasSamplerInfo, nullptr, &canvasSampler);
    mDeleter.enqueue(
        [this, canvasSampler]() { vkDestroySampler(mDevice, canvasSampler, nullptr); });

    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        // Create this descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.pNext              = nullptr;
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &mGlobalSetLayout;

        H_CHECK(vkAllocateDescriptorSets(mDevice, &allocInfo, &mFrames[i].globalDescriptor),
                "failed to allocate global descriptors");

        VkDescriptorImageInfo canvasImageInfo{};
        canvasImageInfo.sampler     = canvasSampler;
        canvasImageInfo.imageView   = mFrames[i].canvasImage.imageView;
        canvasImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet canvasSetWrite = vk::writeDescriptorImage(
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, mFrames[i].globalDescriptor, &canvasImageInfo, 1);

        std::array<VkWriteDescriptorSet, 1> writes = {canvasSetWrite};

        vkUpdateDescriptorSets(mDevice, writes.size(), writes.data(), 0, nullptr);
    }
}

void BdptRenderer::createPipeline()
{
    H_LOG("...creating main draw pipeline");

    VkPipelineShaderStageCreateInfo mainStageInfo =
        vk::createShaderStage(mDevice, kMainShaderName, VK_SHADER_STAGE_COMPUTE_BIT);

    VkPipelineLayoutCreateInfo mainLayoutInfo = vk::pipelineLayoutInfo();
    mainLayoutInfo.setLayoutCount             = 1;
    mainLayoutInfo.pSetLayouts                = &mGlobalSetLayout;
    mainLayoutInfo.pushConstantRangeCount     = 0;
    mainLayoutInfo.pPushConstantRanges        = nullptr;

    H_CHECK(vkCreatePipelineLayout(mDevice, &mainLayoutInfo, nullptr, &mBdptPipelineLayout),
            "Failed to create pipeline layout");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying pipeline layout object");
        vkDestroyPipelineLayout(mDevice, mBdptPipelineLayout, nullptr);
    });

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext  = nullptr;
    pipelineInfo.layout = mBdptPipelineLayout;
    pipelineInfo.stage  = mainStageInfo;

    H_CHECK(vkCreateComputePipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                     &mBdptPipeline),
            "Failed to create compute pipeline");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying compute pipeline");
        vkDestroyPipeline(mDevice, mBdptPipeline, nullptr);
    });

    vkDestroyShaderModule(mDevice, mainStageInfo.module, nullptr);
}

void BdptRenderer::Exit() {}

void BdptRenderer::OnRender()
{
    recordCommandBuffer(mCurrentApplicationFrame->commandBuffer, mCurrentFrameIndex);
    ++mFrameCount;
}
void BdptRenderer::OnImGuiRender()
{
    ImGui::Text("You are viewing the BDPT renderer.");
    ImGui::Text("Move around with WASD, LSHIFT and LCTRL");
    ImGui::Text("Look around with arrow keys. Zoom in/out with mouse wheel");
}

void BdptRenderer::draw(const VkCommandBuffer &commandBuffer)
{
    TracyVkZoneC(mCurrentApplicationFrame->tracyContext, commandBuffer, "draw", tracy::Color::Blue);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mBdptPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mBdptPipelineLayout, 0,
                            1, &mFrames[mCurrentFrameIndex].globalDescriptor, 0, nullptr);
    vkCmdDispatch(commandBuffer, mSwapchainExtent.width, mSwapchainExtent.height, 1);
}

void BdptRenderer::transferCanvasToSwapchain(const VkCommandBuffer &commandBuffer)
{
    VkImageMemoryBarrier barrier;
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = mFrames[mCurrentFrameIndex].canvasImage.image.image;
    barrier.pNext               = VK_NULL_HANDLE;

    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image         = mCurrentSwapchainImage;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkImageCopy region;
    region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel       = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount     = 1;
    region.srcOffset.x                   = 0;
    region.srcOffset.y                   = 0;
    region.srcOffset.z                   = 0;

    region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel       = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount     = 1;
    region.dstOffset.x                   = 0;
    region.dstOffset.y                   = 0;
    region.dstOffset.z                   = 0;

    region.extent.width  = mSwapchainExtent.width;
    region.extent.height = mSwapchainExtent.height;
    region.extent.depth  = 1;

    vkCmdCopyImage(commandBuffer, mFrames[mCurrentFrameIndex].canvasImage.image.image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mCurrentSwapchainImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.image         = mCurrentSwapchainImage;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &barrier);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image         = mFrames[mCurrentFrameIndex].canvasImage.image.image;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

void BdptRenderer::recordCommandBuffer(const VkCommandBuffer &commandBuffer,
                                       [[maybe_unused]] uint32_t imageIndex)
{
    ZoneScopedC(tracy::Color::PeachPuff);

    draw(commandBuffer);

    transferCanvasToSwapchain(commandBuffer);
}

BdptRenderer::~BdptRenderer()
{
    H_LOG("Destroying Bdpt Renderer...");
    H_LOG("...waiting for device to be idle");
    vkDeviceWaitIdle(mDevice);

    mDeleter.flush();
}

}  // namespace hatgpu
