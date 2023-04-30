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

    createCanvas();
    createPipeline();
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
            VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            imageExtent);
        imageInfo.flags = VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
        VkFormatProperties result;

        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &result);

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
    }

    mDeleter.enqueue([this]() {
        for (FrameData &frame : mFrames)
        {
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

void BdptRenderer::createPipeline()
{
    H_LOG("...creating main draw pipeline");

    VkPipelineShaderStageCreateInfo mainStageInfo =
        vk::createShaderStage(mDevice, kMainShaderName, VK_SHADER_STAGE_COMPUTE_BIT);

    VkPipelineLayoutCreateInfo mainLayoutInfo = vk::pipelineLayoutInfo();
    mainLayoutInfo.setLayoutCount             = 1;
    mainLayoutInfo.pSetLayouts                = &mGlobalDescriptorLayout;
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
