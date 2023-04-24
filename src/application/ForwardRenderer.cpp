#include "hatpch.h"

#include "ForwardRenderer.h"
#include "imgui.h"
#include "util/Random.h"
#include "vk/initializers.h"
#include "vk/shader.h"

#include <glm/gtx/string_cast.hpp>
#include <tracy/Tracy.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace hatgpu
{
namespace
{
struct MeshPushConstants
{
    glm::mat4x4 renderMatrix;
};

struct GpuCameraData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec3 position;
};

struct GpuObjectData
{
    glm::mat4 modelTransform;
};

struct GpuPointLight
{
    glm::vec4 position;
    glm::vec4 color;
};

struct GpuDirLight
{
    glm::vec4 direction;
    glm::vec4 color;
};
}  // namespace

ForwardRenderer::ForwardRenderer(const std::string &scenePath)
    : Application("HatGPU"), mScenePath(scenePath)
{}

void ForwardRenderer::Init()
{
    H_LOG("Initializing forward renderer...");
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = mPhysicalDevice;
    allocatorInfo.device                 = mDevice;
    allocatorInfo.instance               = mInstance;

    H_LOG("...creating VMA allocator");
    mAllocator = vk::Allocator(allocatorInfo, mDeleter);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying VMA allocator");
        mAllocator.destroy();
    });

    createDepthImage();
    createRenderPass();
    loadSceneFromDisk();
    createDescriptors();
    createGraphicsPipeline();
    createFramebuffers(mRenderPass);

    uploadSceneToGpu();
}

void ForwardRenderer::Exit() {}

void ForwardRenderer::OnRender()
{
    recordCommandBuffer(mCurrentApplicationFrame->commandBuffer, mCurrentFrameIndex);
    ++mFrameCount;
}
void ForwardRenderer::OnImGuiRender()
{
    ImGui::ShowDemoWindow();
}

void ForwardRenderer::OnRecreateSwapchain()
{
    createFramebuffers(mRenderPass);
}

void ForwardRenderer::createFramebuffers(const VkRenderPass &renderPass)
{
    H_LOG("...creating framebuffers");
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        std::array<VkImageView, 2> attachments = {mSwapchainImageViews[i], mDepthImageView};

        VkFramebufferCreateInfo framebufferInfo = vk::framebufferInfo(renderPass, mSwapchainExtent);
        framebufferInfo.attachmentCount         = attachments.size();
        framebufferInfo.pAttachments            = attachments.data();

        H_CHECK(vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapchainFramebuffers[i]),
                "Failed to create framebuffer");
    }
}

void ForwardRenderer::createDepthImage()
{
    H_LOG("...creating depth image");
    VkExtent3D depthImageExtent = {mSwapchainExtent.width, mSwapchainExtent.height, 1};

    VkImageCreateInfo imageInfo =
        vk::imageInfo(kDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    H_CHECK(vmaCreateImage(mAllocator.Impl, &imageInfo, &allocationInfo, &mDepthImage.image,
                           &mDepthImage.allocation, nullptr),
            "Failed to allocate depth image");

    VkImageViewCreateInfo viewInfo =
        vk::imageViewInfo(kDepthFormat, mDepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    H_CHECK(vkCreateImageView(mDevice, &viewInfo, nullptr, &mDepthImageView),
            "Failed to create depth image view");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying depth image");
        vkDestroyImageView(mDevice, mDepthImageView, nullptr);
        vmaDestroyImage(mAllocator.Impl, mDepthImage.image, mDepthImage.allocation);
    });
}

void ForwardRenderer::createDescriptors()
{
    H_LOG("...creating descriptors");
    std::vector<VkDescriptorPoolSize> sizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
                                               {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 190},
                                               {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10}};

    // Creating the descriptor pool
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = 0;
    poolInfo.maxSets       = 300;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    poolInfo.pPoolSizes    = sizes.data();

    vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool);

    // Creating the global descriptor set, which contains camera info as well as all the object
    // transforms
    VkDescriptorSetLayoutBinding cameraBufferBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    VkDescriptorSetLayoutBinding objectBufferBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
    VkDescriptorSetLayoutBinding dirLightBufferBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
    VkDescriptorSetLayoutBinding lightBufferBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3);

    std::array<VkDescriptorSetLayoutBinding, 4> layoutBindings = {
        cameraBufferBinding, objectBufferBinding, dirLightBufferBinding, lightBufferBinding};

    VkDescriptorSetLayoutCreateInfo globalCreateInfo{};
    globalCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    globalCreateInfo.pNext        = nullptr;
    globalCreateInfo.bindingCount = layoutBindings.size();
    globalCreateInfo.pBindings    = layoutBindings.data();
    globalCreateInfo.flags        = 0;

    H_CHECK(vkCreateDescriptorSetLayout(mDevice, &globalCreateInfo, nullptr, &mGlobalSetLayout),
            "Unable to create global descriptor set layout");

    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        constexpr int kMaxObjects = 10000;
        mFrames[i].objectBuffer   = mAllocator.createBuffer(sizeof(GpuObjectData) * kMaxObjects,
                                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                            VMA_MEMORY_USAGE_CPU_TO_GPU);
        mFrames[i].cameraBuffer   = mAllocator.createBuffer(
              sizeof(GpuCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        mFrames[i].dirLightBuffer = mAllocator.createBuffer(
            sizeof(GpuDirLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Create this descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.pNext              = nullptr;
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &mGlobalSetLayout;

        vkAllocateDescriptorSets(mDevice, &allocInfo, &mFrames[i].globalDescriptor);

        // Use GpuCameraData for the first binding
        VkDescriptorBufferInfo cameraBufferInfo{};
        cameraBufferInfo.buffer = mFrames[i].cameraBuffer.buffer;
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet cameraSetWrite = vk::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mFrames[i].globalDescriptor, &cameraBufferInfo, 0);

        // Use GpuObjectData for the second binding, which is an SSBO
        VkDescriptorBufferInfo objBufferInfo{};
        objBufferInfo.buffer = mFrames[i].objectBuffer.buffer;
        objBufferInfo.offset = 0;
        objBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet objSetWrite = vk::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mFrames[i].globalDescriptor, &objBufferInfo, 1);

        // Use GpuDirLight for the third binding
        VkDescriptorBufferInfo dirLightBufferInfo{};
        dirLightBufferInfo.buffer = mFrames[i].dirLightBuffer.buffer;
        dirLightBufferInfo.offset = 0;
        dirLightBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet dirLightSetWrite = vk::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mFrames[i].globalDescriptor, &dirLightBufferInfo, 2);

        const size_t kLightBufferSize =
            sizeof(GpuPointLight) * std::max(static_cast<size_t>(1), mScene.pointLights.size());
        mFrames[i].lightBuffer = mAllocator.createBuffer(
            kLightBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Write the light buffer info
        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.buffer = mFrames[i].lightBuffer.buffer;
        lightBufferInfo.offset = 0;
        lightBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet lightSetWrite = vk::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mFrames[i].globalDescriptor, &lightBufferInfo, 3);

        std::array<VkWriteDescriptorSet, 4> writes = {cameraSetWrite, objSetWrite, dirLightSetWrite,
                                                      lightSetWrite};

        vkUpdateDescriptorSets(mDevice, writes.size(), writes.data(), 0, nullptr);
    }

    VkDescriptorSetLayoutBinding albedoBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    VkDescriptorSetLayoutBinding metallicRoughnessBinding = vk::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    std::array<VkDescriptorSetLayoutBinding, 2> textureBindings = {albedoBinding,
                                                                   metallicRoughnessBinding};

    VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
    textureLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutInfo.pNext        = nullptr;
    textureLayoutInfo.bindingCount = textureBindings.size();
    textureLayoutInfo.pBindings    = textureBindings.data();
    textureLayoutInfo.flags        = 0;

    vkCreateDescriptorSetLayout(mDevice, &textureLayoutInfo, nullptr, &mTextureSetLayout);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying descriptor sets");
        vkDestroyDescriptorSetLayout(mDevice, mGlobalSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mTextureSetLayout, nullptr);
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

        H_LOG("...destroying buffers");
        for (size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            mAllocator.destroyBuffer(mFrames[i].objectBuffer);
            mAllocator.destroyBuffer(mFrames[i].cameraBuffer);
            mAllocator.destroyBuffer(mFrames[i].dirLightBuffer);
            mAllocator.destroyBuffer(mFrames[i].lightBuffer);
        }
    });
}

void ForwardRenderer::createGraphicsPipeline()
{
    H_LOG("...creating graphics pipeline");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        vk::createShaderStage(mDevice, "../shaders/bin/shader.vert.spv",
                              VK_SHADER_STAGE_VERTEX_BIT),
        vk::createShaderStage(mDevice, "../shaders/bin/shader.frag.spv",
                              VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = vk::vertexInputInfo();

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions    = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly =
        vk::inputAssemblyInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

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

    VkPipelineRasterizationStateCreateInfo rasterizer = vk::rasterizationInfo(VK_POLYGON_MODE_FILL);
    rasterizer.cullMode                               = VK_CULL_MODE_BACK_BIT;

    VkPipelineMultisampleStateCreateInfo multisampling = vk::multisampleInfo();

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
        vk::pipelineDepthStencilInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = vk::colorBlendAttachmentState();
    colorBlendAttachment.blendEnable                         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor                 = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor                 = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp                        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp                        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vk::pipelineLayoutInfo();
    pipelineLayoutCreateInfo.pushConstantRangeCount     = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges        = nullptr;
    std::array<VkDescriptorSetLayout, 2> layouts        = {mGlobalSetLayout, mTextureSetLayout};
    pipelineLayoutCreateInfo.setLayoutCount             = layouts.size();
    pipelineLayoutCreateInfo.pSetLayouts                = layouts.data();

    H_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, nullptr,
                                   &mGraphicsPipelineLayout),
            "Failed to create pipeline layout object");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying graphics pipeline layout");
        vkDestroyPipelineLayout(mDevice, mGraphicsPipelineLayout, nullptr);
    });

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

    pipelineInfo.layout     = mGraphicsPipelineLayout;
    pipelineInfo.renderPass = mRenderPass;
    pipelineInfo.subpass    = 0;

    H_CHECK(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &mGraphicsPipeline),
            "Failed to create graphics pipeline");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying graphics pipeline");
        vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
    });

    for (const auto &stage : shaderStages)
    {
        vkDestroyShaderModule(mDevice, stage.module, nullptr);
    }
}

void ForwardRenderer::createRenderPass()
{
    H_LOG("...creating render pass");
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
    depthAttachment.format         = kDepthFormat;
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

    H_CHECK(vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass),
            "Failed to create render pass");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying render pass");
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    });
}

void ForwardRenderer::uploadMesh(Mesh &mesh)
{
    // Uploading the vertex data followed by the index data in contiguous memory
    const size_t verticesSize = mesh.vertices.size() * sizeof(Vertex);
    const size_t indicesSize  = mesh.indices.size() * sizeof(Mesh::IndexType);
    const size_t bufferSize   = verticesSize + indicesSize;

    vk::AllocatedBuffer stagingBuffer = mAllocator.createBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void *data = mAllocator.map(stagingBuffer);
    std::memcpy(data, mesh.vertices.data(), verticesSize);
    std::memcpy(static_cast<char *>(data) + verticesSize, mesh.indices.data(), indicesSize);
    mAllocator.unmap(stagingBuffer);

    mesh.vertexBuffer = mAllocator.createBuffer(
        verticesSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh.indexBuffer = mAllocator.createBuffer(
        verticesSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

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

    mDeleter.enqueue([this, mesh]() {
        mAllocator.destroyBuffer(mesh.vertexBuffer);
        mAllocator.destroyBuffer(mesh.indexBuffer);
    });

    mAllocator.destroyBuffer(stagingBuffer);
}

void ForwardRenderer::uploadTextures(Mesh &mesh)
{
    // Check whether linear GPU blitting (required by mipmaps) is even supported by this hardware
    // before we do all sorts of work
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, VK_FORMAT_R8G8B8A8_SRGB,
                                        &formatProperties);
    H_ASSERT(
        formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
        "Texture image format does not support linear blitting");

    // Each mesh has its own descriptor set allocated from the global pool
    VkDescriptorSetAllocateInfo textureDescriptorInfo{};
    textureDescriptorInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    textureDescriptorInfo.pNext              = nullptr;
    textureDescriptorInfo.descriptorPool     = mDescriptorPool;
    textureDescriptorInfo.descriptorSetCount = 1;
    textureDescriptorInfo.pSetLayouts        = &mTextureSetLayout;
    vkAllocateDescriptorSets(mDevice, &textureDescriptorInfo, &mesh.descriptor);

    // We go through all of a mesh's textures and upload them to the GPU
    for (const auto &[typ, path] : mesh.textures)
    {
        // If we've already uploaded this mesh's texture we can skip it
        if (mGpuTextures.contains(path))
            continue;

        std::shared_ptr<Texture> cpuTexture = mTextureManager.textures[path];
        // Create staging buffer for image
        VkDeviceSize imageSize            = cpuTexture->width * cpuTexture->height * 4;
        vk::AllocatedBuffer stagingBuffer = mAllocator.createBuffer(
            imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        // Write the CPU texture data into the staging buffer
        void *data = mAllocator.map(stagingBuffer);
        std::memcpy(data, cpuTexture->pixels, imageSize);
        mAllocator.unmap(stagingBuffer);

        // Grab the dimensions of the texture
        VkExtent3D imageExtent{};
        imageExtent.width  = cpuTexture->width;
        imageExtent.height = cpuTexture->height;
        imageExtent.depth  = 1;

        // Calculate the number of mip levels
        int width      = cpuTexture->width;
        int height     = cpuTexture->height;
        auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

        // Create the actual image on the GPU that we'll be using
        VkImageCreateInfo imageInfo =
            vk::imageInfo(VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                          imageExtent, mipLevels);
        vk::AllocatedImage newImage;
        VmaAllocationCreateInfo imgAllocInfo{};
        imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(mAllocator.Impl, &imageInfo, &imgAllocInfo, &newImage.image,
                       &newImage.allocation, nullptr);

        // Use GPU commands to transfer the data from the staging buffer
        // and create the mip levels with blits
        immediateSubmit([&](VkCommandBuffer cmd) {
            // Make a barrier so that we are ready to write to our destination texture
            VkImageSubresourceRange range;
            range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel   = 0;
            range.levelCount     = mipLevels;
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

            // Wait on this barrier so that the texture is ready to be written
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

            // The barrier that we'll use for our mip map generation
            VkImageMemoryBarrier barrier{};
            barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image                           = newImage.image;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;
            barrier.subresourceRange.levelCount     = 1;

            int32_t mipWidth  = width;
            int32_t mipHeight = height;
            for (uint32_t i = 1; i < mipLevels; ++i)
            {
                barrier.subresourceRange.baseMipLevel = i - 1;
                barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
                // We want to transition the previous mip level to a VK_IMAGE_LAYOUT_TRANSFER_SRC
                // layout so that we can use it as source for the blit operation
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                     &barrier);

                VkImageBlit blit{};
                blit.srcOffsets[0]             = {0, 0, 0};
                blit.srcOffsets[1]             = {mipWidth, mipHeight, 1};
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                // we are sourcing from the last mip level
                blit.srcSubresource.mipLevel       = i - 1;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount     = 1;
                blit.dstOffsets[0]                 = {0, 0, 0};
                blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1,
                                      mipHeight > 1 ? mipHeight / 2 : 1, 1};
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                // our destination is the current mip level
                blit.dstSubresource.mipLevel       = i;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount     = 1;

                // Perform the blit operation
                vkCmdBlitImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                               VK_FILTER_LINEAR);

                // Now we transfer the old source mip level to a
                // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL layout since we'll be using it in the
                // fragment shader next
                barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                // We are doing this as the transfer pipeline stage and we will need this image next
                // in the fragment shader stage
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                                     nullptr, 1, &barrier);

                if (mipWidth > 1)
                    mipWidth /= 2;
                if (mipHeight > 1)
                    mipHeight /= 2;
            }

            // Now we finally transfer the last mip level to be
            // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL since that wasn't handled by the loop.
            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                                 1, &barrier);
        });

        // Write the GPU texture we just created to the cache
        GpuTexture &dstTexture = mGpuTextures[path];
        dstTexture.mipLevels   = mipLevels;
        dstTexture.image       = newImage;

        // Get rid of the staging buffer, queue the deletion operation for the GPU image
        mAllocator.destroyBuffer(stagingBuffer);
        mDeleter.enqueue([this, newImage]() {
            vmaDestroyImage(mAllocator.Impl, newImage.image, newImage.allocation);
        });

        // Create the image view
        VkImageViewCreateInfo imageViewInfo = vk::imageViewInfo(
            VK_FORMAT_R8G8B8A8_SRGB, newImage.image, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
        vkCreateImageView(mDevice, &imageViewInfo, nullptr, &dstTexture.imageView);
    }

    for (const auto &[typ, path] : mesh.textures)
    {
        VkSamplerCreateInfo samplerInfo = vk::samplerInfo(VK_FILTER_LINEAR);
        samplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.maxLod              = mGpuTextures[path].mipLevels;
        samplerInfo.anisotropyEnable    = VK_TRUE;
        samplerInfo.mipLodBias          = -0.5f;
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        VkSampler sampler;
        vkCreateSampler(mDevice, &samplerInfo, nullptr, &sampler);
        mDeleter.enqueue([this, sampler]() { vkDestroySampler(mDevice, sampler, nullptr); });

        VkDescriptorImageInfo imageBufferInfo{};
        imageBufferInfo.sampler     = sampler;
        imageBufferInfo.imageView   = mGpuTextures[path].imageView;
        imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        uint32_t dstBinding = 0;
        switch (typ)
        {
            case TextureType::ALBEDO:
                dstBinding = 0;
                break;
            case TextureType::METALLIC_ROUGHNESS:
                dstBinding = 1;
                break;
        }
        VkWriteDescriptorSet descriptorWrite =
            vk::writeDescriptorImage(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mesh.descriptor,
                                     &imageBufferInfo, dstBinding);
        vkUpdateDescriptorSets(mDevice, 1, &descriptorWrite, 0, nullptr);
    }
}

void ForwardRenderer::loadSceneFromDisk()
{
    H_LOG("...loading scene from disk");
    mScene.loadFromJson(mScenePath, mTextureManager);
}

void ForwardRenderer::uploadSceneToGpu()
{
    H_LOG("...uploading scene to GPU");
    for (auto &renderable : mScene.renderables)
    {
        for (auto &mesh : renderable.model->meshes)
        {
            uploadMesh(mesh);
            uploadTextures(mesh);
        }
    }

    mDeleter.enqueue([this]() {
        H_LOG("...destroying texture image views");
        for (const auto &[_, texture] : mGpuTextures)
        {
            vkDestroyImageView(mDevice, texture.imageView, nullptr);
        }
    });
}

void ForwardRenderer::drawObjects(const VkCommandBuffer &commandBuffer)
{
    TracyVkZoneC(mCurrentApplicationFrame->tracyContext, commandBuffer, "drawObjects",
                 tracy::Color::Blue);
    const glm::mat4 view     = mCamera.GetViewMatrix();
    const glm::mat4 proj     = mCamera.GetProjectionMatrix();
    const glm::mat4 viewproj = proj * view;

    GpuCameraData cameraData;
    cameraData.proj     = proj;
    cameraData.view     = view;
    cameraData.viewproj = viewproj;
    cameraData.position = mCamera.Position;

    {
        ZoneScopedNC("Scene buffer writes", tracy::Color::DeepSkyBlue4);
        TracyVkZoneC(mCurrentApplicationFrame->tracyContext, commandBuffer, "Scene buffer writes",
                     tracy::Color::Olive);
        // Writing camera data
        void *data = mAllocator.map(mFrames[mCurrentFrameIndex].cameraBuffer);
        std::memcpy(data, &cameraData, sizeof(GpuCameraData));
        mAllocator.unmap(mFrames[mCurrentFrameIndex].cameraBuffer);

        // Writing all of the object transforms
        data            = mAllocator.map(mFrames[mCurrentFrameIndex].objectBuffer);
        auto objectData = static_cast<GpuObjectData *>(data);
        for (size_t i = 0; i < mScene.renderables.size(); ++i)
        {
            objectData[i].modelTransform = mScene.renderables[i].transform;
        }
        mAllocator.unmap(mFrames[mCurrentFrameIndex].objectBuffer);

        // Writing the dir light
        data                    = mAllocator.map(mFrames[mCurrentFrameIndex].dirLightBuffer);
        auto dirLightData       = static_cast<GpuDirLight *>(data);
        dirLightData->direction = glm::vec4(mScene.dirLight.direction, 0.f);
        dirLightData->color     = glm::vec4(mScene.dirLight.color, 0.f);
        mAllocator.unmap(mFrames[mCurrentFrameIndex].dirLightBuffer);

        // Writing the lights to the light buffer
        data                 = mAllocator.map(mFrames[mCurrentFrameIndex].lightBuffer);
        auto lightBufferData = static_cast<GpuPointLight *>(data);
        for (size_t i = 0; i < mScene.pointLights.size(); ++i)
        {
            lightBufferData[i].position = glm::vec4(mScene.pointLights[i].position, 0.f);
            lightBufferData[i].color    = glm::vec4(mScene.pointLights[i].color, 0.f);
        }
        mAllocator.unmap(mFrames[mCurrentFrameIndex].lightBuffer);
    }

    VkRenderPassBeginInfo renderPassInfo = vk::renderPassBeginInfo(
        mRenderPass, mSwapchainExtent, mSwapchainFramebuffers[mCurrentImageIndex]);

    {
        TracyVkZoneC(mCurrentApplicationFrame->tracyContext, commandBuffer, "Renderpass Begin",
                     tracy::Color::LavenderBlush);
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkClearValue depthClear{};
        depthClear.depthStencil.depth           = 1.f;
        std::array<VkClearValue, 2> clearValues = {clearColor, depthClear};
        renderPassInfo.clearValueCount          = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues             = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

        std::array<VkDescriptorSet, 1> descriptorSets = {
            mFrames[mCurrentFrameIndex].globalDescriptor};
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mGraphicsPipelineLayout, 0, descriptorSets.size(),
                                descriptorSets.data(), 0, nullptr);

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
    }

    for (const auto &object : mScene.renderables)
    {
        ZoneScopedC(tracy::Color::AntiqueWhite);

        for (auto &mesh : object.model->meshes)
        {
            ZoneScopedC(tracy::Color::DodgerBlue);
            TracyVkZoneC(mCurrentApplicationFrame->tracyContext,
                         mCurrentApplicationFrame->commandBuffer, "Mesh Draw", tracy::Color::Red);
            if (!mesh.textures.contains(TextureType::ALBEDO) ||
                !mesh.textures.contains(TextureType::METALLIC_ROUGHNESS))
                continue;

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &offset);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mGraphicsPipelineLayout, 1, 1, &mesh.descriptor, 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(commandBuffer);
}

void ForwardRenderer::recordCommandBuffer(const VkCommandBuffer &commandBuffer,
                                          [[maybe_unused]] uint32_t imageIndex)
{
    ZoneScopedC(tracy::Color::PeachPuff);

    drawObjects(commandBuffer);
}

ForwardRenderer::~ForwardRenderer()
{
    H_LOG("Destroying Forward Renderer...");
    H_LOG("...waiting for device to be idle");
    vkDeviceWaitIdle(mDevice);

    mDeleter.flush();
}

}  // namespace hatgpu
