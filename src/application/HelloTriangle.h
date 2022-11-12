#ifndef _INCLUDE_HELLOTRIANGLE_H
#define _INCLUDE_HELLOTRIANGLE_H

#include "application/Application.h"

namespace efvk
{

class HelloTriangle : public Application
{
  public:
    HelloTriangle();

    virtual void Init() override;
    virtual void Exit() override;

    virtual void OnRender() override;
    virtual void OnImGuiRender() override;

  private:
    void createRenderPass();
    void createGraphicsPipeline();
    void createCommandPool();
    void createCommandBuffer();
    void recordCommandBuffer(const VkCommandBuffer &commandBuffer, uint32_t imageIndex);

    VkRenderPass mRenderPass;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mGraphicsPipeline;
};

}  // namespace efvk

#endif
