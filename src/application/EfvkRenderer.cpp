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

    createDepthImage();
    createRenderPass();
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

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;

    info.imageType = VK_IMAGE_TYPE_2D;

    info.format = mDepthFormat;
    info.extent = depthImageExtent;

    info.mipLevels   = 1;
    info.arrayLayers = 1;
    info.samples     = VK_SAMPLE_COUNT_1_BIT;
    info.tiling      = VK_IMAGE_TILING_OPTIMAL;
    info.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vmaCreateImage(mAllocator, &info, &allocationInfo, &mDepthImage.image,
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

void EfvkRenderer::uploadMesh(Mesh &mesh)
{
    std::cout << "uploading mesh\n";
    // Create vertex buffers
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = mesh.vertices.size() * sizeof(Vertex);
    std::cout << "buffer size " << bufferInfo.size << '\n';
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(mAllocator, &bufferInfo, &vmaAllocInfo, &mesh.vertexBuffer.buffer,
                        &mesh.vertexBuffer.allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate vertex buffer while uploading a mesh");
    }

    mDeleters.emplace_back([this, mesh]() {
        vmaDestroyBuffer(mAllocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation);
    });

    void *data;
    vmaMapMemory(mAllocator, mesh.vertexBuffer.allocation, &data);
    std::memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(mAllocator, mesh.vertexBuffer.allocation);

    VkBufferCreateInfo iboInfo{};
    iboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    iboInfo.size  = mesh.indices.size() * sizeof(mesh.indices[0]);
    iboInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VmaAllocationCreateInfo iboAllocInfo{};
    iboAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(mAllocator, &iboInfo, &iboAllocInfo, &mesh.indexBuffer.buffer,
                        &mesh.indexBuffer.allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate index buffer while uploading a mesh");
    }

    mDeleters.emplace_back([this, mesh] {
        vmaDestroyBuffer(mAllocator, mesh.indexBuffer.buffer, mesh.indexBuffer.allocation);
    });

    vmaMapMemory(mAllocator, mesh.indexBuffer.allocation, &data);
    std::memcpy(data, mesh.indices.data(), mesh.indices.size() * sizeof(mesh.indices[0]));
    vmaUnmapMemory(mAllocator, mesh.indexBuffer.allocation);
}

void EfvkRenderer::createScene()
{
    RenderObject monkey1;
    monkey1.model = std::make_shared<Model>();
    monkey1.model->loadFromObj("../assets/monkey/monkey_smooth.obj");
    for (auto &mesh : monkey1.model->meshes)
    {
        std::cout << "number of verts in mesh: " << mesh.vertices.size() << '\n';
    }
    for (auto &mesh : monkey1.model->meshes)
    {
        uploadMesh(mesh);
    }

    monkey1.transform = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -5.f));
    monkey1.transform = glm::rotate(monkey1.transform, 135.f, glm::vec3(1.f, 0.f, 0.f));
    mRenderables.push_back(monkey1);

    RenderObject sponza;
    sponza.model = std::make_shared<Model>();
    sponza.model->loadFromObj("../assets/sponza/sponza.obj");
    for (auto &mesh : sponza.model->meshes)
    {
        std::cout << "number of verts in mesh: " << mesh.vertices.size() << '\n';
    }

    for (auto &mesh : sponza.model->meshes)
    {
        uploadMesh(mesh);
    }

    auto scale       = glm::scale(glm::mat4(1.f), glm::vec3(0.01f));
    auto rotate      = glm::rotate(glm::mat4(1.f), 180.f, glm::vec3(1.f, 0.f, 0.f));
    auto translate   = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 5.f, 0.f));
    sponza.transform = translate * rotate * scale;
    mRenderables.push_back(sponza);
}

void EfvkRenderer::drawObjects(const VkCommandBuffer &commandBuffer)
{
    const glm::mat4 view = mCamera.GetViewMatrix();
    const glm::mat4 proj = mCamera.GetProjectionMatrix();
    const glm::mat4 vp   = proj * view;

    for (const auto &object : mRenderables)
    {
        glm::mat4 mvp = vp * object.transform;

        MeshPushConstants constants;
        constants.renderMatrix = mvp;

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
}  // namespace efvk