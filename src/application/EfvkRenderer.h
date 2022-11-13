#ifndef _INCLUDE_EFVKRENDERER_H
#define _INCLUDE_EFVKRENDERER_H
#include "efpch.h"

#include "application/Application.h"
#include "geometry/Mesh.h"
#include "scene/Camera.h"

#include <vector>

namespace efvk
{

class EfvkRenderer : public Application
{
  public:
    EfvkRenderer();

    void Init() override;
    void Exit() override;

    void OnRender() override;
    void OnImGuiRender() override;
    void OnRecreateSwapchain() override;

  private:
    void createRenderPass();
    void createGraphicsPipeline();
    void createDepthImage();
    void createFramebuffers(const VkRenderPass &renderPass);
    void recordCommandBuffer(const VkCommandBuffer &commandBuffer, uint32_t imageIndex);

    void loadMeshes();
    void uploadMesh(Mesh &mesh);

    VkRenderPass mRenderPass;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mGraphicsPipeline;
    VmaAllocator mAllocator;

    VkImageView mDepthImageView;
    AllocatedImage mDepthImage{};
    VkFormat mDepthFormat;

    Mesh mTriangleMesh;
    Mesh mMonkeyMesh;

    uint32_t mFrameCount{0};
};

}  // namespace efvk

#endif
