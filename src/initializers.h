#ifndef _INCLUDE_INITIALIZERS_H
#define _INCLUDE_INITIALIZERS_H
#include "efpch.h"

namespace efvk
{
namespace init
{
VkCommandPoolCreateInfo commandPoolInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags = 0);

VkCommandBufferAllocateInfo commandBufferAllocInfo(
    VkCommandPool pool,
    uint32_t count             = 1,
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags = 0);

VkFramebufferCreateInfo framebufferInfo(VkRenderPass renderPass, VkExtent2D extent);

VkFenceCreateInfo fenceInfo(VkFenceCreateFlags = 0);

VkSemaphoreCreateInfo semaphoreInfo(VkSemaphoreCreateFlags = 0);

VkSubmitInfo submitInfo(VkCommandBuffer *cmd);

VkPresentInfoKHR presentInfo();

VkRenderPassBeginInfo renderPassBeginInfo(VkRenderPass renderPass,
                                          VkExtent2D windowExtent,
                                          VkFramebuffer framebuffer);

VkPipelineShaderStageCreateInfo pipelineShaderStageInfo(VkShaderStageFlagBits stage,
                                                        VkShaderModule module);

VkPipelineVertexInputStateCreateInfo vertexInputInfo();

VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo(VkPrimitiveTopology topology);

VkPipelineRasterizationStateCreateInfo rasterizationInfo(VkPolygonMode polygonMode);

VkPipelineMultisampleStateCreateInfo multisampleInfo();

VkPipelineColorBlendAttachmentState colorBlendAttachmentState();

VkPipelineLayoutCreateInfo pipelineLayoutInfo();

VkImageCreateInfo imageInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

VkImageViewCreateInfo imageViewInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilInfo(bool bDepthTest,
                                                               bool bDepthWrite,
                                                               VkCompareOp compareOp);

VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(VkDescriptorType type,
                                                        VkShaderStageFlags stageFlags,
                                                        uint32_t binding);

VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type,
                                           VkDescriptorSet dstSet,
                                           VkDescriptorBufferInfo *bufferInfo,
                                           uint32_t binding);

VkWriteDescriptorSet writeDescriptorImage(VkDescriptorType type,
                                          VkDescriptorSet dstSet,
                                          VkDescriptorImageInfo *imageInfo,
                                          uint32_t binding);

VkSamplerCreateInfo samplerInfo(
    VkFilter filters,
    VkSamplerAddressMode samplerAdressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
}  // namespace init
}  // namespace efvk

#endif  //_INCLUDE_INITIALIZERS_H
