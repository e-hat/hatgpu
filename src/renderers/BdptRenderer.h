#ifndef _INCLUDE_BDPTRENDERER_H
#define _INCLUDE_BDPTRENDERER_H
#include <vulkan/vulkan_core.h>
#include "hatpch.h"

#include "application/Constants.h"
#include "application/Layer.h"
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

namespace hatgpu
{

class BdptRenderer : public Layer
{
  public:
    BdptRenderer(std::shared_ptr<vk::Ctx> ctx, std::shared_ptr<Scene> scene);

    void OnAttach() override;
    void OnDetach() override;
    void OnRender(DrawCtx &drawCtx) override;
    void OnImGuiRender() override;

    static const LayerRequirements kRequirements;

  private:
    void draw(DrawCtx &drawCtx);
    void recordCommandBuffer(DrawCtx &drawCtx);

    void createDescriptorPool();
    void createDescriptorLayout();
    void createDescriptorSets();
    void createCanvas();
    void createPipeline();

    VkDescriptorSetLayout mGlobalSetLayout;

    VkPipelineLayout mBdptPipelineLayout;
    VkPipeline mBdptPipeline;

    vk::Allocator mAllocator;

    void transferCanvasToSwapchain(DrawCtx &drawCtx);

    VkDescriptorPool mDescriptorPool;
    struct FrameData
    {
        VkDescriptorSet globalDescriptor;
        vk::GpuTexture canvasImage;
        vk::AllocatedBuffer rayGenConstantsBuffer;
    };
    std::array<FrameData, constants::kMaxFramesInFlight> mFrames;

    size_t mFrameCount{0};
};

}  // namespace hatgpu

#endif
