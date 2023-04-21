#include "efpch.h"

#include "initializers.h"

namespace hatgpu
{
namespace init
{
VkCommandPoolCreateInfo commandPoolInfo([[maybe_unused]] uint32_t queueFamilyIndex,
                                        VkCommandPoolCreateFlags flags /*= 0*/)
{
    VkCommandPoolCreateInfo info = {};
    info.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext                   = nullptr;

    info.flags = flags;
    return info;
}

VkCommandBufferAllocateInfo commandBufferAllocInfo(
    VkCommandPool pool,
    uint32_t count /*= 1*/,
    VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext                       = nullptr;

    info.commandPool        = pool;
    info.commandBufferCount = count;
    info.level              = level;
    return info;
}

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags /*= 0*/)
{
    VkCommandBufferBeginInfo info = {};
    info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext                    = nullptr;

    info.pInheritanceInfo = nullptr;
    info.flags            = flags;
    return info;
}

VkFramebufferCreateInfo framebufferInfo(VkRenderPass renderPass, VkExtent2D extent)
{
    VkFramebufferCreateInfo info = {};
    info.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext                   = nullptr;

    info.renderPass      = renderPass;
    info.attachmentCount = 1;
    info.width           = extent.width;
    info.height          = extent.height;
    info.layers          = 1;

    return info;
}

VkFenceCreateInfo fenceInfo(VkFenceCreateFlags flags /*= 0*/)
{
    VkFenceCreateInfo info = {};
    info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext             = nullptr;

    info.flags = flags;

    return info;
}

VkSemaphoreCreateInfo semaphoreInfo(VkSemaphoreCreateFlags flags /*= 0*/)
{
    VkSemaphoreCreateInfo info = {};
    info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext                 = nullptr;
    info.flags                 = flags;
    return info;
}

VkSubmitInfo submitInfo(VkCommandBuffer *cmd)
{
    VkSubmitInfo info = {};
    info.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext        = nullptr;

    info.waitSemaphoreCount   = 0;
    info.pWaitSemaphores      = nullptr;
    info.pWaitDstStageMask    = nullptr;
    info.commandBufferCount   = 1;
    info.pCommandBuffers      = cmd;
    info.signalSemaphoreCount = 0;
    info.pSignalSemaphores    = nullptr;

    return info;
}

VkPresentInfoKHR presentInfo()
{
    VkPresentInfoKHR info = {};
    info.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext            = nullptr;

    info.swapchainCount     = 0;
    info.pSwapchains        = nullptr;
    info.pWaitSemaphores    = nullptr;
    info.waitSemaphoreCount = 0;
    info.pImageIndices      = nullptr;

    return info;
}

VkRenderPassBeginInfo renderPassBeginInfo(VkRenderPass renderPass,
                                          VkExtent2D windowExtent,
                                          VkFramebuffer framebuffer)
{
    VkRenderPassBeginInfo info = {};
    info.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext                 = nullptr;

    info.renderPass          = renderPass;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent   = windowExtent;
    info.clearValueCount     = 1;
    info.pClearValues        = nullptr;
    info.framebuffer         = framebuffer;

    return info;
}

VkPipelineShaderStageCreateInfo pipelineShaderStageInfo(VkShaderStageFlagBits stage,
                                                        VkShaderModule shaderModule)
{
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext = nullptr;

    // shader stage
    info.stage = stage;
    // module containing the code for this shader stage
    info.module = shaderModule;
    // the entry point of the shader
    info.pName = "main";
    return info;
}
VkPipelineVertexInputStateCreateInfo vertexInputInfo()
{
    VkPipelineVertexInputStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.pNext = nullptr;

    // no vertex bindings or attributes
    info.vertexBindingDescriptionCount   = 0;
    info.vertexAttributeDescriptionCount = 0;
    return info;
}

VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo(VkPrimitiveTopology topology)
{
    VkPipelineInputAssemblyStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.topology               = topology;
    info.primitiveRestartEnable = VK_FALSE;
    return info;
}
VkPipelineRasterizationStateCreateInfo rasterizationInfo(VkPolygonMode polygonMode)
{
    VkPipelineRasterizationStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.depthClampEnable = VK_FALSE;
    // rasterizer discard allows objects with holes, default to no
    info.rasterizerDiscardEnable = VK_FALSE;

    info.polygonMode = polygonMode;
    info.lineWidth   = 1.0f;
    // no backface cull
    info.cullMode  = VK_CULL_MODE_NONE;
    info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    // no depth bias
    info.depthBiasEnable         = VK_FALSE;
    info.depthBiasConstantFactor = 0.0f;
    info.depthBiasClamp          = 0.0f;
    info.depthBiasSlopeFactor    = 0.0f;

    return info;
}
VkPipelineMultisampleStateCreateInfo multisampleInfo()
{
    VkPipelineMultisampleStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.sampleShadingEnable = VK_FALSE;
    // multisampling defaulted to no multisampling (1 sample per pixel)
    info.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    info.minSampleShading      = 1.0f;
    info.pSampleMask           = nullptr;
    info.alphaToCoverageEnable = VK_FALSE;
    info.alphaToOneEnable      = VK_FALSE;
    return info;
}
VkPipelineColorBlendAttachmentState colorBlendAttachmentState()
{
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    return colorBlendAttachment;
}
VkPipelineLayoutCreateInfo pipelineLayoutInfo()
{
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;

    // empty defaults
    info.flags                  = 0;
    info.setLayoutCount         = 0;
    info.pSetLayouts            = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges    = nullptr;
    return info;
}

VkImageCreateInfo imageInfo(VkFormat format,
                            VkImageUsageFlags usageFlags,
                            VkExtent3D extent,
                            uint32_t mipLevels /*= 1*/)
{
    VkImageCreateInfo info = {};
    info.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext             = nullptr;

    info.imageType = VK_IMAGE_TYPE_2D;

    info.format = format;
    info.extent = extent;

    info.mipLevels   = mipLevels;
    info.arrayLayers = 1;
    info.samples     = VK_SAMPLE_COUNT_1_BIT;
    info.tiling      = VK_IMAGE_TILING_OPTIMAL;
    info.usage       = usageFlags;

    return info;
}

VkImageViewCreateInfo imageViewInfo(VkFormat format,
                                    VkImage image,
                                    VkImageAspectFlags aspectFlags,
                                    uint32_t mipLevels /* = 1*/)
{
    // build a image-view for the depth image to use for rendering
    VkImageViewCreateInfo info = {};
    info.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext                 = nullptr;

    info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    info.image                           = image;
    info.format                          = format;
    info.subresourceRange.baseMipLevel   = 0;
    info.subresourceRange.levelCount     = mipLevels;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount     = 1;
    info.subresourceRange.aspectMask     = aspectFlags;

    return info;
}

VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilInfo(bool bDepthTest,
                                                               bool bDepthWrite,
                                                               VkCompareOp compareOp)
{
    VkPipelineDepthStencilStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.depthTestEnable       = bDepthTest ? VK_TRUE : VK_FALSE;
    info.depthWriteEnable      = bDepthWrite ? VK_TRUE : VK_FALSE;
    info.depthCompareOp        = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
    info.depthBoundsTestEnable = VK_FALSE;
    info.minDepthBounds        = 0.0f;  // Optional
    info.maxDepthBounds        = 1.0f;  // Optional
    info.stencilTestEnable     = VK_FALSE;

    return info;
}

VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(VkDescriptorType type,
                                                        VkShaderStageFlags stageFlags,
                                                        uint32_t binding)
{
    VkDescriptorSetLayoutBinding setbind = {};
    setbind.binding                      = binding;
    setbind.descriptorCount              = 1;
    setbind.descriptorType               = type;
    setbind.pImmutableSamplers           = nullptr;
    setbind.stageFlags                   = stageFlags;

    return setbind;
}
VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type,
                                           VkDescriptorSet dstSet,
                                           VkDescriptorBufferInfo *bufferInfo,
                                           uint32_t binding)
{
    VkWriteDescriptorSet write = {};
    write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext                = nullptr;

    write.dstBinding      = binding;
    write.dstSet          = dstSet;
    write.descriptorCount = 1;
    write.descriptorType  = type;
    write.pBufferInfo     = bufferInfo;

    return write;
}

VkWriteDescriptorSet writeDescriptorImage(VkDescriptorType type,
                                          VkDescriptorSet dstSet,
                                          VkDescriptorImageInfo *imageInfo,
                                          uint32_t binding)
{
    VkWriteDescriptorSet write = {};
    write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext                = nullptr;

    write.dstBinding      = binding;
    write.dstSet          = dstSet;
    write.descriptorCount = 1;
    write.descriptorType  = type;
    write.pImageInfo      = imageInfo;

    return write;
}

VkSamplerCreateInfo samplerInfo(
    VkFilter filters,
    VkSamplerAddressMode samplerAdressMode /*= VK_SAMPLER_ADDRESS_MODE_REPEAT*/)
{
    VkSamplerCreateInfo info = {};
    info.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext               = nullptr;

    info.magFilter    = filters;
    info.minFilter    = filters;
    info.addressModeU = samplerAdressMode;
    info.addressModeV = samplerAdressMode;
    info.addressModeW = samplerAdressMode;

    return info;
}
}  // namespace init
}  // namespace hatgpu