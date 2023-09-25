#ifndef _INCLUDE_FORWARDRENDERER_H
#define _INCLUDE_FORWARDRENDERER_H
#include "hatpch.h"

#include "application/Application.h"
#include "geometry/Model.h"
#include "renderers/layers/AabbLayer.h"
#include "scene/Camera.h"
#include "scene/Scene.h"
#include "texture/Texture.h"
#include "ui/Toggle.h"
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

class ForwardRenderer : public Application
{
  public:
    ForwardRenderer(const std::string &scenePath);
    ~ForwardRenderer() override;

    void Init() override;
    void Exit() override;

    void OnRender() override;
    void OnImGuiRender() override;

  protected:
    virtual VkImageUsageFlags swapchainImageUsage() const override
    {
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

  private:
    void createDescriptors();
    void createGraphicsPipeline();
    void loadSceneFromDisk();
    void uploadSceneToGpu();

    void drawObjects();
    void recordCommandBuffer();

    void uploadTextures(Mesh &mesh);

    VkPipelineLayout mGraphicsPipelineLayout;
    VkPipeline mGraphicsPipeline;

    VkDescriptorSetLayout mGlobalSetLayout;
    VkDescriptorSetLayout mTextureSetLayout;

    VkDescriptorPool mDescriptorPool;
    struct FrameData
    {
        VkDescriptorSet globalDescriptor;
        vk::AllocatedBuffer cameraBuffer;
        vk::AllocatedBuffer objectBuffer;
        vk::AllocatedBuffer dirLightBuffer;
        vk::AllocatedBuffer lightBuffer;
    };
    std::array<FrameData, kMaxFramesInFlight> mFrames;

    ui::Toggle mAabbLayerToggle;

    TextureManager mTextureManager;
    std::unordered_map<std::string, vk::GpuTexture> mGpuTextures;

    uint32_t mFrameCount{0};

    std::string mScenePath;
    Scene mScene;

    vk::DeletionQueue mDeleter;

    std::shared_ptr<AabbLayer> mAabbLayer;
};

}  // namespace hatgpu

#endif
