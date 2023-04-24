#ifndef _INCLUDE_BDPTRENDERER_H
#define _INCLUDE_BDPTRENDERER_H
#include "hatpch.h"

#include "application/Application.h"
#include "geometry/Model.h"
#include "scene/Camera.h"
#include "scene/Scene.h"
#include "vk/allocator.h"
#include "vk/deleter.h"
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
    BdptRenderer(const std::string &scenePath);
    ~BdptRenderer() override;

    void Init() override;
    void Exit() override;

    void OnRender() override;
    void OnImGuiRender() override;
    void OnRecreateSwapchain() override;

  private:
    void createRenderPass();
    void createDescriptors();
    void createGraphicsPipeline();
    void createDepthImage();
    void createFramebuffers(const VkRenderPass &renderPass);
    void loadSceneFromDisk();
    void uploadSceneToGpu();

    vk::AllocatedBuffer createBuffer(size_t allocSize,
                                     VkBufferUsageFlags usage,
                                     VmaMemoryUsage memoryUsage);

    void drawObjects(const VkCommandBuffer &commandBuffer);
    void recordCommandBuffer(const VkCommandBuffer &commandBuffer, uint32_t imageIndex);

    void uploadMesh(Mesh &mesh);
    void uploadTextures(Mesh &mesh);

    VkRenderPass mRenderPass;
    VkPipelineLayout mGraphicsPipelineLayout;
    VkPipeline mGraphicsPipeline;
    vk::Allocator mAllocator;

    VkImageView mDepthImageView;
    vk::AllocatedImage mDepthImage{};

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

    TextureManager mTextureManager;
    struct GpuTexture
    {
        vk::AllocatedImage image;
        VkImageView imageView;
        uint32_t mipLevels;
    };
    std::unordered_map<std::string, GpuTexture> mGpuTextures;

    uint32_t mFrameCount{0};

    std::string mScenePath;
    Scene mScene;

    vk::DeletionQueue mDeleter;
};

}  // namespace hatgpu

#endif
