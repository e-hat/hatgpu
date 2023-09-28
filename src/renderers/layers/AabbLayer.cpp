#include <vulkan/vulkan_core.h>

#include "AabbLayer.h"

#include "imgui.h"
#include "vk/deleter.h"
#include "vk/initializers.h"
#include "vk/shader.h"

namespace hatgpu
{
namespace
{
constexpr const char *kVertexShaderName = "../shaders/bin/layers/aabb.vert.spv";

struct PushConstants
{
    glm::mat4 renderMatrix;
};
}  // namespace

const LayerRequirements AabbLayer::kRequirements = []() -> LayerRequirements {
    LayerRequirements result{
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        {VK_KHR_SWAPCHAIN_EXTENSION_NAME},
    };
    result.deviceFeatures.fillModeNonSolid = VK_TRUE;
    return result;
}();

void AabbLayer::OnAttach()
{
    H_LOG("Attaching AabbLayer");

    if (mInitialized)
    {
        H_LOG("...already initialized.");
        return;
    }

    H_LOG("...creating draw pipeline");
    VkPipelineShaderStageCreateInfo vertexStageInfo =
        vk::createShaderStage(mCtx->device, kVertexShaderName, VK_SHADER_STAGE_VERTEX_BIT);

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(glm::vec3);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attributeDescription{};
    attributeDescription.binding  = 0;
    attributeDescription.location = 0;
    attributeDescription.format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescription.offset   = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = vk::vertexInputInfo();
    vertexInputInfo.vertexBindingDescriptionCount        = 1;
    vertexInputInfo.pVertexBindingDescriptions           = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount      = 1;
    vertexInputInfo.pVertexAttributeDescriptions         = &attributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly =
        vk::inputAssemblyInfo(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(mCtx->swapchainExtent.width);
    viewport.height   = static_cast<float>(mCtx->swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mCtx->swapchainExtent;

    auto viewportState = vk::viewportStateInfo(&viewport, &scissor);

    VkPipelineRasterizationStateCreateInfo rasterizer = vk::rasterizationInfo(VK_POLYGON_MODE_LINE);
    rasterizer.cullMode                               = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisampling = vk::multisampleInfo();

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
        vk::pipelineDepthStencilInfo(false, false, VK_COMPARE_OP_ALWAYS);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = vk::colorBlendAttachmentState();
    colorBlendAttachment.blendEnable                         = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    VkPushConstantRange pushConstant{};
    pushConstant.size       = sizeof(PushConstants);
    pushConstant.offset     = 0;
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vk::pipelineLayoutInfo();
    layoutInfo.pushConstantRangeCount     = 1;
    layoutInfo.pPushConstantRanges        = &pushConstant;
    layoutInfo.setLayoutCount             = 0;
    layoutInfo.pSetLayouts                = nullptr;

    H_CHECK(vkCreatePipelineLayout(mCtx->device, &layoutInfo, nullptr, &mLayout),
            "Failed to create pipeline layout");

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 1;
    pipelineInfo.pStages    = &vertexStageInfo;

    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencilStateCreateInfo;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;

    pipelineInfo.layout     = mLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass    = 0;

    VkPipelineRenderingCreateInfo pipelineCreateRenderingInfo{};
    pipelineCreateRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineCreateRenderingInfo.pNext = nullptr;
    pipelineCreateRenderingInfo.colorAttachmentCount    = 1;
    pipelineCreateRenderingInfo.pColorAttachmentFormats = &mCtx->swapchainImageFormat;
    pipelineCreateRenderingInfo.depthAttachmentFormat   = VK_FORMAT_D32_SFLOAT;

    pipelineInfo.pNext = &pipelineCreateRenderingInfo;

    H_CHECK(vkCreateGraphicsPipelines(mCtx->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &mPipeline),
            "Failed to create graphics pipeline");

    mDeleter.enqueue([this]() {
        H_LOG("Destroying AabbLayer");

        H_LOG("...destroying pipeline layout object");
        vkDestroyPipelineLayout(mCtx->device, mLayout, nullptr);

        H_LOG("...destroying graphics pipeline");
        vkDestroyPipeline(mCtx->device, mPipeline, nullptr);
    });

    vkDestroyShaderModule(mCtx->device, vertexStageInfo.module, nullptr);

    mInitialized = true;
}

void AabbLayer::OnDetach()
{
    H_LOG("Detaching AabbLayer");
    H_LOG("...nothing to do");
}

void AabbLayer::OnImGuiRender() {}

void AabbLayer::OnRender([[maybe_unused]] DrawCtx &drawCtx) {}
}  // namespace hatgpu
