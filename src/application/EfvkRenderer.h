#ifndef _INCLUDE_EFVKRENDERER_H
#define _INCLUDE_EFVKRENDERER_H
#include "efpch.h"

#include "application/Application.h"
#include "geometry/Mesh.h"
#include "scene/Camera.h"

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
    void createScene();

    void drawObjects(const VkCommandBuffer &commandBuffer);
    void recordCommandBuffer(const VkCommandBuffer &commandBuffer, uint32_t imageIndex);

    void loadMeshes();
    void uploadMesh(Mesh &mesh);

    using MeshHandle = std::weak_ptr<Mesh>;
    struct RenderObject
    {
        MeshHandle mesh;
        glm::mat4 transform;
    };

    VkRenderPass mRenderPass;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mGraphicsPipeline;
    VmaAllocator mAllocator;

    VkImageView mDepthImageView;
    AllocatedImage mDepthImage{};
    VkFormat mDepthFormat;

    std::shared_ptr<Mesh> mMonkeyMesh;

    std::vector<RenderObject> mRenderables;
    std::unordered_map<std::string, std::shared_ptr<Mesh>> mMeshes;

    std::optional<MeshHandle> getMesh(const std::string &name);

    uint32_t mFrameCount{0};
};

}  // namespace efvk

#endif
