#include <vulkan/vulkan_core.h>

#include "AabbLayer.h"

#include "glm/gtx/string_cast.hpp"
#include "imgui.h"
#include "vk/deleter.h"
#include "vk/initializers.h"
#include "vk/shader.h"

namespace hatgpu
{
namespace
{
constexpr const char *kVertexShaderName   = "../shaders/bin/aabb/shader.vert.spv";
constexpr const char *kFragmentShaderName = "../shaders/bin/aabb/shader.frag.spv";

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

void AabbLayer::Init()
{
    H_LOG("...creating draw pipeline");
    VkPipelineShaderStageCreateInfo vertexStageInfo =
        vk::createShaderStage(mCtx->device, kVertexShaderName, VK_SHADER_STAGE_VERTEX_BIT);

    VkPipelineShaderStageCreateInfo fragmentStageInfo =
        vk::createShaderStage(mCtx->device, kFragmentShaderName, VK_SHADER_STAGE_FRAGMENT_BIT);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageInfos = {vertexStageInfo,
                                                                       fragmentStageInfo};

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(glm::vec4);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attributeDescription{};
    attributeDescription.binding  = 0;
    attributeDescription.location = 0;
    attributeDescription.format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescription.offset   = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = vk::vertexInputInfo();
    vertexInputInfo.vertexBindingDescriptionCount        = 1;
    vertexInputInfo.pVertexBindingDescriptions           = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount      = 1;
    vertexInputInfo.pVertexAttributeDescriptions         = &attributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly =
        vk::inputAssemblyInfo(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

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
    pipelineInfo.stageCount = shaderStageInfos.size();
    pipelineInfo.pStages    = shaderStageInfos.data();

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

    for (const auto &shaderStageInfo : shaderStageInfos)
    {
        vkDestroyShaderModule(mCtx->device, shaderStageInfo.module, nullptr);
    }

    createGeometry();
    uploadGeometry();
    mDeleter.enqueue([this]() {
        mCtx->allocator.destroyBuffer(mVertexBuffer);
        mCtx->allocator.destroyBuffer(mIndexBuffer);
    });

    mInitialized = true;
}

void AabbLayer::createGeometry()
{
    mVertices.clear();
    mIndices.clear();

    for (const auto &renderObj : mScene->renderables)
    {
        mVertices.reserve(mVertices.size() + 8 * renderObj.model->meshes.size());
        mIndices.reserve(mIndices.size() + 24 * renderObj.model->meshes.size());
        for (const auto &mesh : renderObj.model->meshes)
        {
            Aabb box              = mesh.BoundingBox(renderObj.transform);
            Mesh::IndexType first = mVertices.size();

            // left bottom front, 0
            mVertices.push_back(glm::vec4(box.min.x, box.min.y, box.min.z, 1.0));
            // left bottom back, 1
            mVertices.push_back(glm::vec4(box.min.x, box.min.y, box.max.z, 1.0));
            // left top front, 2
            mVertices.push_back(glm::vec4(box.min.x, box.max.y, box.min.z, 1.0));
            // left top back, 3
            mVertices.push_back(glm::vec4(box.min.x, box.max.y, box.max.z, 1.0));
            // right bottom front, 4
            mVertices.push_back(glm::vec4(box.max.x, box.min.y, box.min.z, 1.0));
            // right bottom back, 5
            mVertices.push_back(glm::vec4(box.max.x, box.min.y, box.max.z, 1.0));
            // right top front, 6
            mVertices.push_back(glm::vec4(box.max.x, box.max.y, box.min.z, 1.0));
            // right top back, 7
            mVertices.push_back(glm::vec4(box.max.x, box.max.y, box.max.z, 1.0));

            mIndices.push_back(first);
            mIndices.push_back(first + 4);
            mIndices.push_back(first + 4);
            mIndices.push_back(first + 6);
            mIndices.push_back(first + 6);
            mIndices.push_back(first + 2);
            mIndices.push_back(first + 2);
            mIndices.push_back(first);

            mIndices.push_back(first);
            mIndices.push_back(first + 1);
            mIndices.push_back(first + 1);
            mIndices.push_back(first + 3);
            mIndices.push_back(first + 3);
            mIndices.push_back(first + 2);

            mIndices.push_back(first + 1);
            mIndices.push_back(first + 5);
            mIndices.push_back(first + 5);
            mIndices.push_back(first + 7);
            mIndices.push_back(first + 7);
            mIndices.push_back(first + 3);

            mIndices.push_back(first + 6);
            mIndices.push_back(first + 7);
            mIndices.push_back(first + 4);
            mIndices.push_back(first + 5);
        }
    }
}

void AabbLayer::uploadGeometry()
{
    // Uploading the vertex data followed by the index data in contiguous memory
    const size_t verticesSize = mVertices.size() * sizeof(glm::vec4);
    const size_t indicesSize  = mIndices.size() * sizeof(Mesh::IndexType);

    vk::Allocator allocator = mCtx->allocator;

    vk::AllocatedBuffer vertexStagingBuffer = allocator.createBuffer(
        verticesSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    vk::AllocatedBuffer indexStagingBuffer = allocator.createBuffer(
        indicesSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void *data = allocator.map(vertexStagingBuffer);
    std::memcpy(data, mVertices.data(), verticesSize);
    /*glm::vec4 *dataGlm = reinterpret_cast<glm::vec4 *>(data);
    for (size_t i = 0; i < mVertices.size(); ++i)
    {
        LOGGER.error("{}: {}", i, glm::to_string(dataGlm[i]));
    }*/
    ;
    allocator.unmap(vertexStagingBuffer);

    data = allocator.map(indexStagingBuffer);
    std::memcpy(data, mIndices.data(), indicesSize);
    allocator.unmap(indexStagingBuffer);

    mVertexBuffer = allocator.createBuffer(
        verticesSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mIndexBuffer = allocator.createBuffer(
        indicesSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    mCtx->uploadContext.immediateSubmit([=, this](VkCommandBuffer cmd) {
        VkBufferCopy vboCopy{};
        vboCopy.dstOffset = 0;
        vboCopy.srcOffset = 0;
        vboCopy.size      = verticesSize;
        vkCmdCopyBuffer(cmd, vertexStagingBuffer.buffer, mVertexBuffer.buffer, 1, &vboCopy);

        VkBufferCopy iboCopy{};
        iboCopy.dstOffset = 0;
        iboCopy.srcOffset = 0;
        iboCopy.size      = indicesSize;
        vkCmdCopyBuffer(cmd, indexStagingBuffer.buffer, mIndexBuffer.buffer, 1, &iboCopy);
    });

    allocator.destroyBuffer(vertexStagingBuffer);
    allocator.destroyBuffer(indexStagingBuffer);
}

void AabbLayer::OnDetach()
{
    H_LOG("Detaching AabbLayer");
    H_LOG("...nothing to do");
}

void AabbLayer::OnImGuiRender() {}

void AabbLayer::OnRender(DrawCtx &drawCtx)
{
    ZoneScopedC(tracy::Color::AntiqueWhite);
    VkZoneC("OnRender - AabbLayer", tracy::Color::Blue);

    {
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView   = drawCtx.swapchainImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo renderInfo{};
        renderInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderInfo.flags                = 0;
        renderInfo.renderArea           = {.offset = {0, 0}, .extent = mCtx->swapchainExtent};
        renderInfo.layerCount           = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments    = &colorAttachment;
        renderInfo.pDepthAttachment     = nullptr;

        vkCmdBeginRendering(drawCtx.commandBuffer, &renderInfo);
        vkCmdBindPipeline(drawCtx.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(mCtx->swapchainExtent.width);
        viewport.height   = static_cast<float>(mCtx->swapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(drawCtx.commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = mCtx->swapchainExtent;
        vkCmdSetScissor(drawCtx.commandBuffer, 0, 1, &scissor);
    }

    {
        VkZoneC("AabbLayer Draw", tracy::Color::Red);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(drawCtx.commandBuffer, 0, 1, &mVertexBuffer.buffer, &offset);
        vkCmdBindIndexBuffer(drawCtx.commandBuffer, mIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        PushConstants pushConstants{};
        pushConstants.renderMatrix =
            mScene->camera.GetProjectionMatrix() * mScene->camera.GetViewMatrix();

        vkCmdPushConstants(drawCtx.commandBuffer, mLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(PushConstants), &pushConstants);

        vkCmdDrawIndexed(drawCtx.commandBuffer, static_cast<uint32_t>(mIndices.size()), 1, 0, 0, 0);
    }

    vkCmdEndRendering(drawCtx.commandBuffer);
}
}  // namespace hatgpu
