#ifndef _INCLUDE_EFVKRENDERER_H
#define _INCLUDE_EFVKRENDERER_H
#include "efpch.h"

#include "application/Application.h"
#include "geometry/Model.h"
#include "scene/Camera.h"

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

    struct RenderObject
    {
        std::shared_ptr<Model> model;
        glm::mat4 transform;
    };

    VkRenderPass mRenderPass;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mGraphicsPipeline;
    VmaAllocator mAllocator;

    VkImageView mDepthImageView;
    AllocatedImage mDepthImage{};
    VkFormat mDepthFormat;

    VkDescriptorSetLayout mGlobalSetLayout;
    VkDescriptorPool mDescriptorPool;
    std::vector<AllocatedBuffer> mCameraBuffers;
    std::vector<VkDescriptorSet> mGlobalDescriptors;

    std::vector<RenderObject> mRenderables;

    uint32_t mFrameCount{0};

    std::deque<Deleter> mDeleters;
};

}  // namespace efvk

#endif
