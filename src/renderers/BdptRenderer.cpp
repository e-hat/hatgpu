#include "application/Layer.h"
#include "hatpch.h"

#include "BdptRenderer.h"
#include "imgui.h"
#include "scene/Scene.h"
#include "texture/Texture.h"
#include "util/Random.h"
#include "vk/initializers.h"
#include "vk/shader.h"

#include <glm/gtx/string_cast.hpp>
#include <tracy/Tracy.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>

namespace
{

static constexpr const char *kMainShaderName = "../shaders/bin/bdpt/main.comp.spv";

struct GpuRayGenConstants
{
    glm::vec4 origin;
    glm::vec4 horizontal;
    glm::vec4 vertical;
    glm::vec4 lowerLeftCorner;

    glm::uvec2 viewportExtent;
};

GpuRayGenConstants makeGpuRayGenConstants(size_t width, size_t height)
{
    float aspectRatio = static_cast<float>(width) / height;

    float viewportHeight = 2.0f;
    float viewportWidth  = aspectRatio * viewportHeight;
    // TODO: Probably put this in the camera class, or somehow make this configurable?
    float focalLength = 1.0f;

    GpuRayGenConstants result{};
    result.origin          = glm::vec4(0.f);
    result.horizontal      = glm::vec4(viewportWidth, 0.f, 0.f, 0.f);
    result.vertical        = glm::vec4(0.f, viewportHeight, 0.f, 0.f);
    result.lowerLeftCorner = result.origin - result.horizontal * 0.5f - result.vertical * 0.5f -
                             glm::vec4(0.f, 0.f, focalLength, 0.f);
    result.viewportExtent = glm::uvec2(width, height);

    return result;
}

constexpr size_t kCanvasBindingLocation          = 0;
constexpr size_t kRayGenConstantsBindingLocation = 1;

}  // namespace

namespace hatgpu
{

BdptRenderer::BdptRenderer(std::shared_ptr<vk::Ctx> ctx, std::shared_ptr<Scene> scene)
    : Layer("HatGPU", ctx, scene)
{}

const LayerRequirements BdptRenderer::kRequirements = []() -> LayerRequirements {
    LayerRequirements result{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                             {VK_KHR_SWAPCHAIN_EXTENSION_NAME}};
    return result;
}();

void BdptRenderer::OnAttach()
{

    H_LOG("Initializing bdpt renderer...");
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = mCtx->physicalDevice;
    allocatorInfo.device                 = mCtx->device;
    allocatorInfo.instance               = mCtx->instance;

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

void BdptRenderer::OnDetach() {}

void BdptRenderer::createCanvas()
{
    H_LOG("...creating canvas image");
    for (FrameData &frame : mFrames)
    {
        VkExtent3D imageExtent;
        imageExtent.width           = mCtx->swapchainExtent.width;
        imageExtent.height          = mCtx->swapchainExtent.height;
        imageExtent.depth           = 1;
        VkImageCreateInfo imageInfo = vk::imageInfo(
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            imageExtent);
        imageInfo.flags = VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;

        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(mCtx->physicalDevice, VK_FORMAT_R8G8B8A8_UNORM,
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

        mCtx->uploadContext.immediateSubmit([&frame](VkCommandBuffer commandBuffer) {
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
        H_CHECK(
            vkCreateImageView(mCtx->device, &canvasViewInfo, nullptr, &frame.canvasImage.imageView),
            "failed to create canvas image view");
    }

    mDeleter.enqueue([this]() {
        for (FrameData &frame : mFrames)
        {
            vkDestroyImageView(mCtx->device, frame.canvasImage.imageView, nullptr);
            frame.canvasImage.destroy(mAllocator);
        }
    });
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

    vkCreateDescriptorPool(mCtx->device, &poolInfo, nullptr, &mDescriptorPool);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying descriptor set pool");
        vkDestroyDescriptorPool(mCtx->device, mDescriptorPool, nullptr);
    });
}

void BdptRenderer::createDescriptorLayout()
{
    H_LOG("...creating main descriptor set layout");

    VkDescriptorSetLayoutBinding canvasBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, kCanvasBindingLocation);
    VkDescriptorSetLayoutBinding rayGenConstantsBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
        kRayGenConstantsBindingLocation);

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {canvasBinding, rayGenConstantsBinding};

    VkDescriptorSetLayoutCreateInfo globalLayoutInfo{};
    globalLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    globalLayoutInfo.pNext        = nullptr;
    globalLayoutInfo.bindingCount = bindings.size();
    globalLayoutInfo.pBindings    = bindings.data();
    globalLayoutInfo.flags        = 0;

    vkCreateDescriptorSetLayout(mCtx->device, &globalLayoutInfo, nullptr, &mGlobalSetLayout);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying descriptor set layout");
        vkDestroyDescriptorSetLayout(mCtx->device, mGlobalSetLayout, nullptr);
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
    vkCreateSampler(mCtx->device, &canvasSamplerInfo, nullptr, &canvasSampler);
    mDeleter.enqueue(
        [this, canvasSampler]() { vkDestroySampler(mCtx->device, canvasSampler, nullptr); });

    GpuRayGenConstants gpuRayGenConstants =
        makeGpuRayGenConstants(mCtx->swapchainExtent.width, mCtx->swapchainExtent.height);

    for (size_t i = 0; i < constants::kMaxFramesInFlight; ++i)
    {
        // Create this descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.pNext              = nullptr;
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &mGlobalSetLayout;

        H_CHECK(vkAllocateDescriptorSets(mCtx->device, &allocInfo, &mFrames[i].globalDescriptor),
                "failed to allocate global descriptors");

        VkDescriptorImageInfo canvasImageInfo{};
        canvasImageInfo.sampler     = canvasSampler;
        canvasImageInfo.imageView   = mFrames[i].canvasImage.imageView;
        canvasImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet canvasSetWrite =
            vk::writeDescriptorImage(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, mFrames[i].globalDescriptor,
                                     &canvasImageInfo, kCanvasBindingLocation);

        mFrames[i].rayGenConstantsBuffer =
            mAllocator.createBuffer(sizeof(GpuRayGenConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_CPU_TO_GPU);
        // Writing raygen constant data
        auto data =
            static_cast<GpuRayGenConstants *>(mAllocator.map(mFrames[i].rayGenConstantsBuffer));
        *data = gpuRayGenConstants;
        mAllocator.unmap(mFrames[i].rayGenConstantsBuffer);

        VkDescriptorBufferInfo rayGenConstantsInfo{};
        rayGenConstantsInfo.buffer = mFrames[i].rayGenConstantsBuffer.buffer;
        rayGenConstantsInfo.offset = 0;
        rayGenConstantsInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet rayGenConstantsSetWrite = vk::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mFrames[i].globalDescriptor, &rayGenConstantsInfo,
            kRayGenConstantsBindingLocation);

        std::array<VkWriteDescriptorSet, 2> writes = {canvasSetWrite, rayGenConstantsSetWrite};

        vkUpdateDescriptorSets(mCtx->device, writes.size(), writes.data(), 0, nullptr);
    }

    mDeleter.enqueue([this]() {
        H_LOG("...deleting buffers");
        for (size_t i = 0; i < constants::kMaxFramesInFlight; ++i)
        {
            mAllocator.destroyBuffer(mFrames[i].rayGenConstantsBuffer);
        }
    });
}

void BdptRenderer::createPipeline()
{
    H_LOG("...creating main draw pipeline");

    VkPipelineShaderStageCreateInfo mainStageInfo =
        vk::createShaderStage(mCtx->device, kMainShaderName, VK_SHADER_STAGE_COMPUTE_BIT);

    VkPipelineLayoutCreateInfo mainLayoutInfo = vk::pipelineLayoutInfo();
    mainLayoutInfo.setLayoutCount             = 1;
    mainLayoutInfo.pSetLayouts                = &mGlobalSetLayout;
    mainLayoutInfo.pushConstantRangeCount     = 0;
    mainLayoutInfo.pPushConstantRanges        = nullptr;

    H_CHECK(vkCreatePipelineLayout(mCtx->device, &mainLayoutInfo, nullptr, &mBdptPipelineLayout),
            "Failed to create pipeline layout");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying pipeline layout object");
        vkDestroyPipelineLayout(mCtx->device, mBdptPipelineLayout, nullptr);
    });

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext  = nullptr;
    pipelineInfo.layout = mBdptPipelineLayout;
    pipelineInfo.stage  = mainStageInfo;

    H_CHECK(vkCreateComputePipelines(mCtx->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                     &mBdptPipeline),
            "Failed to create compute pipeline");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying compute pipeline");
        vkDestroyPipeline(mCtx->device, mBdptPipeline, nullptr);
    });

    vkDestroyShaderModule(mCtx->device, mainStageInfo.module, nullptr);
}

void BdptRenderer::OnRender(DrawCtx &drawCtx)
{
    recordCommandBuffer(drawCtx);
    ++mFrameCount;
}
void BdptRenderer::OnImGuiRender()
{
    ImGui::Text("You are viewing the BDPT renderer.");
    ImGui::Text("Move around with WASD, LSHIFT and LCTRL");
    ImGui::Text("Look around with arrow keys. Zoom in/out with mouse wheel");
}

void BdptRenderer::draw(DrawCtx &drawCtx)
{
    VkZoneC("draw", tracy::Color::Blue);

    vkCmdBindPipeline(drawCtx.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mBdptPipeline);
    vkCmdBindDescriptorSets(drawCtx.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            mBdptPipelineLayout, 0, 1,
                            &mFrames[drawCtx.frameIndex].globalDescriptor, 0, nullptr);
    vkCmdDispatch(drawCtx.commandBuffer, mCtx->swapchainExtent.width, mCtx->swapchainExtent.height,
                  1);
}

void BdptRenderer::transferCanvasToSwapchain(DrawCtx &drawCtx)
{
    VkImageMemoryBarrier barrier;
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = mFrames[drawCtx.frameIndex].canvasImage.image.image;
    barrier.pNext               = VK_NULL_HANDLE;

    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(drawCtx.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image         = drawCtx.swapchainImage;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(drawCtx.commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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

    region.extent.width  = mCtx->swapchainExtent.width;
    region.extent.height = mCtx->swapchainExtent.height;
    region.extent.depth  = 1;

    vkCmdCopyImage(drawCtx.commandBuffer, mFrames[drawCtx.frameIndex].canvasImage.image.image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, drawCtx.swapchainImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.image         = drawCtx.swapchainImage;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    vkCmdPipelineBarrier(drawCtx.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &barrier);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image         = mFrames[drawCtx.frameIndex].canvasImage.image.image;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(drawCtx.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

void BdptRenderer::recordCommandBuffer(DrawCtx &drawCtx)
{
    ZoneScopedC(tracy::Color::PeachPuff);

    draw(drawCtx);
    transferCanvasToSwapchain(drawCtx);
}

}  // namespace hatgpu
