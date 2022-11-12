#ifndef _INCLUDE_HELLOTRIANGLE_H
#define _INCLUDE_HELLOTRIANGLE_H

#include "application/Application.h"

#include <vector>

namespace efvk
{

class HelloTriangle : public Application
{
  public:
    HelloTriangle();

    void Init() override;
    void Exit() override;

    void OnRender() override;
    void OnImGuiRender() override;
    void OnRecreateSwapchain() override;

  private:
    void createRenderPass();
    void createGraphicsPipeline();
    void recordCommandBuffer(const VkCommandBuffer &commandBuffer, uint32_t imageIndex);

    VkRenderPass mRenderPass;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mGraphicsPipeline;
};

}  // namespace efvk

#endif
