#ifndef _INCLUDE_BDPTRENDERER_H
#define _INCLUDE_BDPTRENDERER_H
#include <vulkan/vulkan_core.h>
#include "hatpch.h"

#include "application/Application.h"
#include "geometry/Model.h"
#include "scene/Camera.h"
#include "scene/Scene.h"
#include "texture/Texture.h"
#include "vk/allocator.h"
#include "vk/deleter.h"
#include "vk/gpu_texture.h"
#include "vk/types.h"

#include <glm/glm.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hatgpu
{

class BdptRenderer : public Application
{
  public:
    BdptRenderer();
    ~BdptRenderer() override;

    void Init() override;
    void Exit() override;

    void OnRender() override;
    void OnImGuiRender() override;

  protected:
    bool checkDeviceExtensionSupport(const VkPhysicalDevice &device) override;
    inline constexpr virtual VkImageUsageFlags swapchainImageUsage() const override
    {
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    inline constexpr virtual VkSubpassDependency renderSubpassDependency() const override
    {
        VkSubpassDependency dependency{};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependency.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        return dependency;
    }

  private:
    void draw(const VkCommandBuffer &commandBuffer);
    void recordCommandBuffer(const VkCommandBuffer &commandBuffer, uint32_t imageIndex);

    void createDescriptorPool();
    void createDescriptorLayout();
    void createDescriptorSets();
    void createCanvas();
    void createPipeline();

    VkDescriptorSetLayout mGlobalSetLayout;

    VkPipelineLayout mBdptPipelineLayout;
    VkPipeline mBdptPipeline;

    vk::Allocator mAllocator;

    void transferCanvasToSwapchain(const VkCommandBuffer &commandBuffer);

    VkDescriptorPool mDescriptorPool;
    struct FrameData
    {
        VkDescriptorSet globalDescriptor;
        vk::GpuTexture canvasImage;
    };
    std::array<FrameData, kMaxFramesInFlight> mFrames;

    size_t mFrameCount{0};

    vk::DeletionQueue mDeleter;
};

}  // namespace hatgpu

#endif
