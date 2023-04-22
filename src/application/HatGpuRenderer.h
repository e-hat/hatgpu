#ifndef _INCLUDE_HATGPURENDERER_H
#define _INCLUDE_HATGPURENDERER_H
#include "hatpch.h"

#include "application/Application.h"
#include "geometry/Model.h"
#include "scene/Camera.h"
#include "scene/Scene.h"

#include <glm/glm.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hatgpu
{

class HatGpuRenderer : public Application
{
  public:
    HatGpuRenderer(const std::string &scenePath);
    ~HatGpuRenderer() override;

    void Init() override;
    void Exit() override;

    void OnRender() override;
    void OnImGuiRender() override;
    void OnRecreateSwapchain() override;

  private:
    void createUploadContext();
    void createRenderPass();
    void createDescriptors();
    void createGraphicsPipeline();
    void createDepthImage();
    void createFramebuffers(const VkRenderPass &renderPass);
    void loadSceneFromDisk();
    void uploadSceneToGpu();

    AllocatedBuffer createBuffer(size_t allocSize,
                                 VkBufferUsageFlags usage,
                                 VmaMemoryUsage memoryUsage);

    void drawObjects(const VkCommandBuffer &commandBuffer);
    void recordCommandBuffer(const VkCommandBuffer &commandBuffer, uint32_t imageIndex);

    void uploadMesh(Mesh &mesh);
    void uploadTextures(Mesh &mesh);

    struct UploadContext
    {
        VkFence uploadFence;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
    };
    UploadContext mUploadContext;

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function);

    VkRenderPass mRenderPass;
    VkPipelineLayout mGraphicsPipelineLayout;
    VkPipeline mGraphicsPipeline;
    VmaAllocator mAllocator;

    VkImageView mDepthImageView;
    AllocatedImage mDepthImage{};
    VkFormat mDepthFormat;

    VkDescriptorSetLayout mGlobalSetLayout;
    VkDescriptorSetLayout mTextureSetLayout;

    VkDescriptorPool mDescriptorPool;
    struct FrameData
    {
        VkDescriptorSet globalDescriptor;
        AllocatedBuffer cameraBuffer;
        AllocatedBuffer objectBuffer;
        AllocatedBuffer dirLightBuffer;
        AllocatedBuffer lightBuffer;
    };
    std::array<FrameData, kMaxFramesInFlight> mFrames;

    TextureManager mTextureManager;
    struct GpuTexture
    {
        AllocatedImage image;
        VkImageView imageView;
        uint32_t mipLevels;
    };
    std::unordered_map<std::string, GpuTexture> mGpuTextures;

    uint32_t mFrameCount{0};

    std::string mScenePath;
    Scene mScene;

    std::deque<Deleter> mDeleters;
};

}  // namespace hatgpu

#endif
