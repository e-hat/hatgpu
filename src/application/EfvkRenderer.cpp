#include "efpch.h"

#include "EfvkRenderer.h"

#include <glm/gtx/string_cast.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace efvk
{
namespace
{
std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file");
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

struct MeshPushConstants
{
    glm::vec4 data;
    glm::mat4x4 renderMatrix;
};

struct GpuCameraData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
};
}  // namespace

EfvkRenderer::EfvkRenderer() : Application("Efvk Rendering Engine") {}

void EfvkRenderer::Init()
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = mPhysicalDevice;
    allocatorInfo.device                 = mDevice;
    allocatorInfo.instance               = mInstance;

    if (vmaCreateAllocator(&allocatorInfo, &mAllocator) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VMA allocator");
    }

    mDeleters.emplace_back([this]() { vmaDestroyAllocator(mAllocator); });

    createUploadContext();
    createDepthImage();
    createRenderPass();
    createDescriptors();
    createGraphicsPipeline();
    createFramebuffers(mRenderPass);

    createScene();
}

void EfvkRenderer::Exit() {}

void EfvkRenderer::OnRender()
{
    vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrameIndex], VK_TRUE,
                    std::numeric_limits<uint64_t>::max());
    VkResult nextImageResult = vkAcquireNextImageKHR(
        mDevice, mSwapchain, std::numeric_limits<uint64_t>::max(),
        mImageAvailableSemaphores[mCurrentFrameIndex], VK_NULL_HANDLE, &mCurrentImageIndex);
    if (nextImageResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapchain();
        return;
    }
    else if (nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrameIndex]);
    vkResetCommandBuffer(mCommandBuffers[mCurrentFrameIndex], 0);
    recordCommandBuffer(mCommandBuffers[mCurrentFrameIndex], mCurrentImageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    std::array<VkSemaphore, 1> waitSemaphores = {mImageAvailableSemaphores[mCurrentFrameIndex]};
    std::array<VkPipelineStageFlags, 1> waitStages = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores    = waitSemaphores.data();
    submitInfo.pWaitDstStageMask  = waitStages.data();

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &mCommandBuffers[mCurrentFrameIndex];

    std::array<VkSemaphore, 1> signalSemaphores = {mRenderFinishedSemaphores[mCurrentFrameIndex]};
    submitInfo.signalSemaphoreCount             = signalSemaphores.size();
    submitInfo.pSignalSemaphores                = signalSemaphores.data();

    if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrameIndex]) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores.data();

    std::array<VkSwapchainKHR, 1> swapchains = {mSwapchain};
    presentInfo.swapchainCount               = 1;
    presentInfo.pSwapchains                  = swapchains.data();
    presentInfo.pImageIndices                = &mCurrentImageIndex;
    VkResult presentResult                   = vkQueuePresentKHR(mPresentQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
        mFramebufferResized)
    {
        SetFramebufferResized(false);
        recreateSwapchain();
    }
    else if (presentResult != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swapchain image");
    }

    ++mFrameCount;
}
void EfvkRenderer::OnImGuiRender() {}

void EfvkRenderer::OnRecreateSwapchain()
{
    createFramebuffers(mRenderPass);
}

void EfvkRenderer::createFramebuffers(const VkRenderPass &renderPass)
{
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        std::array<VkImageView, 2> attachments = {mSwapchainImageViews[i], mDepthImageView};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = renderPass;
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments    = attachments.data();
        framebufferInfo.width           = mSwapchainExtent.width;
        framebufferInfo.height          = mSwapchainExtent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapchainFramebuffers[i]) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void EfvkRenderer::createDepthImage()
{
    VkExtent3D depthImageExtent = {mSwapchainExtent.width, mSwapchainExtent.height, 1};

    mDepthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;

    imageInfo.imageType = VK_IMAGE_TYPE_2D;

    imageInfo.format = mDepthFormat;
    imageInfo.extent = depthImageExtent;

    imageInfo.mipLevels   = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vmaCreateImage(mAllocator, &imageInfo, &allocationInfo, &mDepthImage.image,
                       &mDepthImage.allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate depth image");
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext                 = nullptr;

    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.image                           = mDepthImage.image;
    viewInfo.format                          = mDepthFormat;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (vkCreateImageView(mDevice, &viewInfo, nullptr, &mDepthImageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth image view");
    }

    mDeleters.emplace_back([this]() {
        vkDestroyImageView(mDevice, mDepthImageView, nullptr);
        vmaDestroyImage(mAllocator, mDepthImage.image, mDepthImage.allocation);
    });
}

void EfvkRenderer::createDescriptors()
{
    std::array<VkDescriptorPoolSize, 1> sizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = 0;
    poolInfo.maxSets       = 10;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    poolInfo.pPoolSizes    = sizes.data();

    vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool);

    VkDescriptorSetLayoutBinding cameraBufferBinding{};
    cameraBufferBinding.binding         = 0;
    cameraBufferBinding.descriptorCount = 1;
    cameraBufferBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBufferBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext        = nullptr;
    layoutCreateInfo.bindingCount = 1;
    layoutCreateInfo.flags        = 0;
    layoutCreateInfo.pBindings    = &cameraBufferBinding;

    vkCreateDescriptorSetLayout(mDevice, &layoutCreateInfo, nullptr, &mGlobalSetLayout);

    mCameraBuffers.resize(kMaxFramesInFlight);
    mGlobalDescriptors.resize(kMaxFramesInFlight);

    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        mCameraBuffers[i] = createBuffer(sizeof(GpuCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.pNext              = nullptr;
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &mGlobalSetLayout;

        vkAllocateDescriptorSets(mDevice, &allocInfo, &mGlobalDescriptors[i]);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = mCameraBuffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(GpuCameraData);

        VkWriteDescriptorSet setWrite{};
        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        setWrite.pNext = nullptr;

        setWrite.dstBinding = 0;
        setWrite.dstSet     = mGlobalDescriptors[i];

        setWrite.descriptorCount = 1;
        setWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        setWrite.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(mDevice, 1, &setWrite, 0, nullptr);
    }

    mDeleters.emplace_back([this]() {
        vkDestroyDescriptorSetLayout(mDevice, mGlobalSetLayout, nullptr);
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            vmaDestroyBuffer(mAllocator, mCameraBuffers[i].buffer, mCameraBuffers[i].allocation);
        }
    });
}

void EfvkRenderer::createGraphicsPipeline()
{
    const auto vertShaderCode = readFile("../shaders/bin/shader.vert.spv");
    const auto fragShaderCode = readFile("../shaders/bin/shader.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertStageCreateInfo{};
    vertStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageCreateInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageCreateInfo.module = vertShaderModule;
    vertStageCreateInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStageCreateInfo{};
    fragStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageCreateInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageCreateInfo.module = fragShaderModule;
    fragStageCreateInfo.pName  = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertStageCreateInfo,
                                                                   fragStageCreateInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions    = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(mSwapchainExtent.width);
    viewport.height   = static_cast<float>(mSwapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mSwapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.pNext = nullptr;
    depthStencilStateCreateInfo.depthTestEnable       = VK_TRUE;
    depthStencilStateCreateInfo.depthWriteEnable      = VK_TRUE;
    depthStencilStateCreateInfo.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_FALSE;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    VkPushConstantRange pushConstant;
    pushConstant.offset     = 0;
    pushConstant.size       = sizeof(MeshPushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;

    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts    = &mGlobalSetLayout;

    if (vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout object");
    }

    mDeleters.emplace_back(
        [this]() { vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr); });

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages    = shaderStages.data();

    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencilStateCreateInfo;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;

    pipelineInfo.layout     = mPipelineLayout;
    pipelineInfo.renderPass = mRenderPass;
    pipelineInfo.subpass    = 0;

    if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &mGraphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    mDeleters.emplace_back([this]() { vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr); });

    vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
}

void EfvkRenderer::createUploadContext()
{
    VkFenceCreateInfo uploadFenceCreateInfo{};
    uploadFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    uploadFenceCreateInfo.flags = 0;

    if (vkCreateFence(mDevice, &uploadFenceCreateInfo, nullptr, &mUploadContext.uploadFence) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create upload context fence");
    }

    mDeleters.emplace_back(
        [this]() { vkDestroyFence(mDevice, mUploadContext.uploadFence, nullptr); });

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = 0;

    if (vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr,
                            &mUploadContext.commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create upload context command pool");
    }

    mDeleters.emplace_back(
        [this]() { vkDestroyCommandPool(mDevice, mUploadContext.commandPool, nullptr); });

    VkCommandBufferAllocateInfo commandBufferAllocInfo{};
    commandBufferAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.pNext              = nullptr;
    commandBufferAllocInfo.commandPool        = mUploadContext.commandPool;
    commandBufferAllocInfo.commandBufferCount = 1;
    commandBufferAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if (vkAllocateCommandBuffers(mDevice, &commandBufferAllocInfo, &mUploadContext.commandBuffer) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate upload context command buffer");
    }
}

void EfvkRenderer::immediateSubmit(std::function<void(VkCommandBuffer)> &&function)
{
    VkCommandBuffer cmd = mUploadContext.commandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo{};
    cmdBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext            = nullptr;
    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &cmdBeginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin command buffer");
    }

    function(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to end command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;

    submitInfo.waitSemaphoreCount   = 0;
    submitInfo.pWaitSemaphores      = nullptr;
    submitInfo.pWaitDstStageMask    = nullptr;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores    = nullptr;

    if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mUploadContext.uploadFence) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit to queue");
    }

    vkWaitForFences(mDevice, 1, &mUploadContext.uploadFence, true,
                    std::numeric_limits<uint32_t>::max());
    vkResetFences(mDevice, 1, &mUploadContext.uploadFence);

    vkResetCommandPool(mDevice, mUploadContext.commandPool, 0);
}

void EfvkRenderer::createRenderPass()
{
    // Initialize color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = mSwapchainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Initialize depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = mDepthFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depthDependency{};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.dstSubpass = 0;
    depthDependency.srcStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = 0;
    depthDependency.dstStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    std::array<VkSubpassDependency, 2> dependencies    = {dependency, depthDependency};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies   = dependencies.data();

    if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create render pass");
    }

    mDeleters.emplace_back([this]() { vkDestroyRenderPass(mDevice, mRenderPass, nullptr); });
}

AllocatedBuffer EfvkRenderer::createBuffer(size_t allocSize,
                                           VkBufferUsageFlags usage,
                                           VmaMemoryUsage memoryUsage)
{
    // allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext              = nullptr;

    bufferInfo.size  = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage                   = memoryUsage;

    AllocatedBuffer newBuffer;

    // allocate the buffer
    if (vmaCreateBuffer(mAllocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer,
                        &newBuffer.allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate new buffer");
    }

    return newBuffer;
}

void EfvkRenderer::uploadMesh(Mesh &mesh)
{
    // Uploading the vertex data followed by the index data in contiguous memory
    const size_t verticesSize = mesh.vertices.size() * sizeof(Vertex);
    const size_t indicesSize  = mesh.indices.size() * sizeof(Mesh::IndexType);
    const size_t bufferSize   = verticesSize + indicesSize;

    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.pNext = nullptr;
    stagingBufferInfo.size  = bufferSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    AllocatedBuffer stagingBuffer;

    if (vmaCreateBuffer(mAllocator, &stagingBufferInfo, &vmaAllocInfo, &stagingBuffer.buffer,
                        &stagingBuffer.allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create staging buffer for mesh data");
    }

    void *data;
    vmaMapMemory(mAllocator, stagingBuffer.allocation, &data);
    auto charPtr = static_cast<char *>(data);
    std::memcpy(data, mesh.vertices.data(), verticesSize);
    std::memcpy(charPtr + verticesSize, mesh.indices.data(), indicesSize);
    vmaUnmapMemory(mAllocator, stagingBuffer.allocation);

    VkBufferCreateInfo vboInfo{};
    vboInfo.sType      = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vboInfo.pNext      = nullptr;
    vboInfo.size       = verticesSize;
    vboInfo.usage      = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(mAllocator, &vboInfo, &vmaAllocInfo, &mesh.vertexBuffer.buffer,
                        &mesh.vertexBuffer.allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate vertex buffer");
    }

    VkBufferCreateInfo iboInfo{};
    iboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    iboInfo.pNext = nullptr;
    iboInfo.size  = indicesSize;
    iboInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (vmaCreateBuffer(mAllocator, &iboInfo, &vmaAllocInfo, &mesh.indexBuffer.buffer,
                        &mesh.indexBuffer.allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate index buffer");
    }

    immediateSubmit([=](VkCommandBuffer cmd) {
        VkBufferCopy vboCopy{};
        vboCopy.dstOffset = 0;
        vboCopy.srcOffset = 0;
        vboCopy.size      = verticesSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1, &vboCopy);

        VkBufferCopy iboCopy{};
        iboCopy.dstOffset = 0;
        iboCopy.srcOffset = verticesSize;
        iboCopy.size      = indicesSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.indexBuffer.buffer, 1, &iboCopy);
    });

    mDeleters.emplace_back([this, mesh]() {
        vmaDestroyBuffer(mAllocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation);
        vmaDestroyBuffer(mAllocator, mesh.indexBuffer.buffer, mesh.indexBuffer.allocation);
    });

    vmaDestroyBuffer(mAllocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

void EfvkRenderer::uploadTextures(Mesh &mesh)
{
    for (const auto &[_, paths] : mesh.textures)
    {
        if (paths.empty())
            continue;

        const std::string &path = paths.front();
        if (mTextureImages.contains(path))
            continue;

        std::shared_ptr<Texture> cpuTexture = mTextureManager.textures[path];
        // Create staging buffer for image
        VkDeviceSize imageSize = cpuTexture->width * cpuTexture->height * 4;
        AllocatedBuffer stagingBuffer =
            createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        void *data;
        vmaMapMemory(mAllocator, stagingBuffer.allocation, &data);
        std::memcpy(data, cpuTexture->pixels, imageSize);
        vmaUnmapMemory(mAllocator, stagingBuffer.allocation);

        VkExtent3D imageExtent{};
        imageExtent.width  = cpuTexture->width;
        imageExtent.height = cpuTexture->height;
        imageExtent.depth  = 1;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext = nullptr;

        imageInfo.imageType = VK_IMAGE_TYPE_2D;

        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.extent = imageExtent;

        imageInfo.mipLevels   = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        AllocatedImage newImage;
        VmaAllocationCreateInfo imgAllocInfo{};
        imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(mAllocator, &imageInfo, &imgAllocInfo, &newImage.image, &newImage.allocation,
                       nullptr);

        immediateSubmit([&](VkCommandBuffer cmd) {
            VkImageSubresourceRange range;
            range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel   = 0;
            range.levelCount     = 1;
            range.baseArrayLayer = 0;
            range.layerCount     = 1;

            VkImageMemoryBarrier imageBarrierTransfer{};
            imageBarrierTransfer.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageBarrierTransfer.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrierTransfer.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrierTransfer.image            = newImage.image;
            imageBarrierTransfer.subresourceRange = range;
            imageBarrierTransfer.srcAccessMask    = 0;
            imageBarrierTransfer.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &imageBarrierTransfer);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset      = 0;
            copyRegion.bufferRowLength   = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel       = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount     = 1;
            copyRegion.imageExtent                     = imageExtent;

            // copy the buffer into the image
            vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage.image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            VkImageMemoryBarrier imageBarrierRead = imageBarrierTransfer;

            imageBarrierRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrierRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            imageBarrierRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrierRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            // barrier the image into the shader readable layout
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                                 1, &imageBarrierRead);
        });

        mTextureImages[path] = newImage;

        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.pNext                           = nullptr;
        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image                           = newImage.image;
        imageViewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
        imageViewInfo.subresourceRange.baseMipLevel   = 0;
        imageViewInfo.subresourceRange.levelCount     = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount     = 1;
        imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageView imageView;
        vkCreateImageView(mDevice, &imageViewInfo, nullptr, &imageView);
        mTextureViews[path] = imageView;

        mDeleters.emplace_back([this, newImage]() {
            vmaDestroyImage(mAllocator, newImage.image, newImage.allocation);
        });

        vmaDestroyBuffer(mAllocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }
}

void EfvkRenderer::createScene()
{
    RenderObject monkey1;
    monkey1.model = std::make_shared<Model>();
    monkey1.model->loadFromObj("../assets/monkey/monkey_smooth.obj", mTextureManager);
    for (auto &mesh : monkey1.model->meshes)
    {
        uploadMesh(mesh);
        uploadTextures(mesh);
    }

    monkey1.transform = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -5.f));
    monkey1.transform = glm::rotate(monkey1.transform, 135.f, glm::vec3(1.f, 0.f, 0.f));
    mRenderables.push_back(monkey1);

    RenderObject sponza;
    sponza.model = std::make_shared<Model>();
    sponza.model->loadFromObj("../assets/sponza/sponza.obj", mTextureManager);

    for (auto &mesh : sponza.model->meshes)
    {
        uploadMesh(mesh);
        uploadTextures(mesh);
    }

    auto scale       = glm::scale(glm::mat4(1.f), glm::vec3(0.01f));
    auto rotate      = glm::rotate(glm::mat4(1.f), 160.5f, glm::vec3(1.f, 0.f, 0.f));
    auto translate   = glm::translate(glm::mat4(1.f), glm::vec3(1.f, 0.f, 0.f));
    sponza.transform = translate * rotate * scale;
    mRenderables.push_back(sponza);
}

void EfvkRenderer::drawObjects(const VkCommandBuffer &commandBuffer)
{
    const glm::mat4 view     = mCamera.GetViewMatrix();
    const glm::mat4 proj     = mCamera.GetProjectionMatrix();
    const glm::mat4 viewproj = proj * view;

    GpuCameraData cameraData;
    cameraData.proj     = proj;
    cameraData.view     = view;
    cameraData.viewproj = viewproj;

    void *data;
    vmaMapMemory(mAllocator, mCameraBuffers[mCurrentFrameIndex].allocation, &data);
    std::memcpy(data, &cameraData, sizeof(GpuCameraData));
    vmaUnmapMemory(mAllocator, mCameraBuffers[mCurrentFrameIndex].allocation);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1,
                            &mGlobalDescriptors[mCurrentFrameIndex], 0, nullptr);

    for (const auto &object : mRenderables)
    {
        MeshPushConstants constants;
        constants.renderMatrix = object.transform;

        vkCmdPushConstants(commandBuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(MeshPushConstants), &constants);

        for (const auto &mesh : object.model->meshes)
        {
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &offset);

            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
        }
    }
}

void EfvkRenderer::recordCommandBuffer(const VkCommandBuffer &commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = mRenderPass;
    renderPassInfo.framebuffer       = mSwapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mSwapchainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkClearValue depthClear{};
    depthClear.depthStencil.depth           = 1.f;
    std::array<VkClearValue, 2> clearValues = {clearColor, depthClear};
    renderPassInfo.clearValueCount          = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues             = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(mSwapchainExtent.width);
    viewport.height   = static_cast<float>(mSwapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mSwapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    drawObjects(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to end recording of command buffer");
    }
}

EfvkRenderer::~EfvkRenderer()
{
    vkDeviceWaitIdle(mDevice);

    while (!mDeleters.empty())
    {
        Deleter nextDeleter = mDeleters.back();
        nextDeleter();
        mDeleters.pop_back();
    }
}

}  // namespace efvk