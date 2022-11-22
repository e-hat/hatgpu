#ifndef _INCLUDE_EFVKRENDERER_H
#define _INCLUDE_EFVKRENDERER_H
#include "efpch.h"

#include "application/Application.h"
#include "geometry/Model.h"
#include "scene/Camera.h"

#include <glm/glm.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace efvk
{

class EfvkRenderer : public Application
{
  public:
    EfvkRenderer();
    ~EfvkRenderer() override;

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
    void createScene();

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
    VkPipelineLayout mPipelineLayout;
    VkPipeline mGraphicsPipeline;
    VmaAllocator mAllocator;

    VkImageView mDepthImageView;
    AllocatedImage mDepthImage{};
    VkFormat mDepthFormat;

    VkDescriptorSetLayout mGlobalSetLayout;
    VkDescriptorSetLayout mClusteringSetLayout;
    VkDescriptorSetLayout mTextureSetLayout;

    VkDescriptorPool mDescriptorPool;

    struct FrameData
    {
        VkDescriptorSet globalDescriptor;
        AllocatedBuffer cameraBuffer;
        AllocatedBuffer objectBuffer;

        VkDescriptorSet clusteringDescriptor;
        AllocatedBuffer lightBuffer;
        AllocatedBuffer clusteringInfoBuffer;
        AllocatedBuffer lightGridBuffer;
        AllocatedBuffer lightIndicesBuffer;
    };
    std::array<FrameData, kMaxFramesInFlight> mFrames;

    struct RenderObject
    {
        std::shared_ptr<Model> model;
        glm::mat4 transform;
    };
    std::vector<RenderObject> mRenderables;
    struct PointLight
    {
        glm::vec3 position;
        glm::vec3 color = glm::vec3(1.f);
    };
    std::vector<PointLight> mPointLights;

    TextureManager mTextureManager;
    struct GpuTexture
    {
        AllocatedImage image;
        VkImageView imageView;
        uint32_t mipLevels;
    };
    std::unordered_map<std::string, GpuTexture> mGpuTextures;

    uint32_t mFrameCount{0};

    std::deque<Deleter> mDeleters;
};

}  // namespace efvk

#endif
