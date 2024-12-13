#ifndef _INCLUDE_FORWARDRENDERER_H
#define _INCLUDE_FORWARDRENDERER_H
#include "hatpch.h"

#include "application/Constants.h"
#include "application/Renderer.h"
#include "geometry/Model.h"
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

namespace hatgpu
{

class ForwardRenderer : public Renderer
{
  public:
    ForwardRenderer(std::shared_ptr<vk::Ctx> ctx, std::shared_ptr<Scene> scene);

    virtual void Init() override;
    void OnDetach() override;
    void OnRender(DrawCtx &drawCtx) override;
    void OnImGuiRender() override;

    static const LayerRequirements kRequirements;

  private:
    void createDescriptors();
    void createGraphicsPipeline();
    void uploadSceneToGpu();

    void drawObjects(DrawCtx &drawCtx);
    void recordCommandBuffer(DrawCtx &drawCtx);

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
    std::array<FrameData, constants::kMaxFramesInFlight> mFrames;

    TextureManager mTextureManager;
    std::unordered_map<std::string, vk::GpuTexture> mGpuTextures;

    uint32_t mFrameCount{0};

    std::string mScenePath;
};

}  // namespace hatgpu

#endif
