#include "efpch.h"

#include "HatGpuRenderer.h"
#include "initializers.h"
#include "util/Random.h"

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

struct ClusteringInfo
{
    glm::mat4 projInverse;
    glm::vec2 screenDimensions;
    // defines view frustrum
    float zFar;
    float zNear;
    float scale;
    float bias;

    // describing 2D tile dimensions
    float tileSizeX;
    float tileSizeY;

    uint numZSlices;
};

struct LightGridEntry
{
    uint offset;
    uint nLights;
};

struct Aabb
{
    glm::vec4 minPt;
    glm::vec4 maxPt;
};

static constexpr uint32_t kNumTilesX   = 16;
static constexpr uint32_t kNumTilesY   = 9;
static constexpr uint32_t kNumSlicesZ  = 24;
static constexpr uint32_t kNumClusters = kNumTilesX * kNumTilesY * kNumSlicesZ;

static constexpr uint32_t kMaxLightsPerCluster = 65;
}  // namespace

HatGpuRenderer::HatGpuRenderer(const std::string &scenePath)
    : Application("Efvk Rendering Engine"), mScenePath(scenePath)
{}

void HatGpuRenderer::Init()
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
    loadSceneFromDisk();
    createDescriptors();
    createGraphicsPipeline();
    createComputePipelines();
    createFramebuffers(mRenderPass);

    uploadSceneToGpu();
    generateAabb();
}

void HatGpuRenderer::Exit() {}

void HatGpuRenderer::OnRender()
{
    recordCommandBuffer(mCurrentApplicationFrame->commandBuffer, mCurrentFrameIndex);
    ++mFrameCount;
}
void HatGpuRenderer::OnImGuiRender() {}

void HatGpuRenderer::OnRecreateSwapchain()
{
    createFramebuffers(mRenderPass);
}

void HatGpuRenderer::createFramebuffers(const VkRenderPass &renderPass)
{
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        std::array<VkImageView, 2> attachments = {mSwapchainImageViews[i], mDepthImageView};

        VkFramebufferCreateInfo framebufferInfo =
            init::framebufferInfo(renderPass, mSwapchainExtent);
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments    = attachments.data();

        if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapchainFramebuffers[i]) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void HatGpuRenderer::createDepthImage()
{
    VkExtent3D depthImageExtent = {mSwapchainExtent.width, mSwapchainExtent.height, 1};

    mDepthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imageInfo = init::imageInfo(
        mDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vmaCreateImage(mAllocator, &imageInfo, &allocationInfo, &mDepthImage.image,
                       &mDepthImage.allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate depth image");
    }

    VkImageViewCreateInfo viewInfo =
        init::imageViewInfo(mDepthFormat, mDepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    if (vkCreateImageView(mDevice, &viewInfo, nullptr, &mDepthImageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth image view");
    }

    mDeleters.emplace_back([this]() {
        vkDestroyImageView(mDevice, mDepthImageView, nullptr);
        vmaDestroyImage(mAllocator, mDepthImage.image, mDepthImage.allocation);
    });
}

void HatGpuRenderer::createDescriptors()
{
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
    VkDescriptorSetLayoutBinding cameraBufferBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0);
    VkDescriptorSetLayoutBinding objectBufferBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
    VkDescriptorSetLayoutBinding dirLightBufferBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);

    std::array<VkDescriptorSetLayoutBinding, 3> layoutBindings = {
        cameraBufferBinding, objectBufferBinding, dirLightBufferBinding};

    VkDescriptorSetLayoutCreateInfo globalCreateInfo{};
    globalCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    globalCreateInfo.pNext        = nullptr;
    globalCreateInfo.bindingCount = layoutBindings.size();
    globalCreateInfo.pBindings    = layoutBindings.data();
    globalCreateInfo.flags        = 0;

    if (vkCreateDescriptorSetLayout(mDevice, &globalCreateInfo, nullptr, &mGlobalSetLayout) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Unable to create global descriptor set layout");
    }

    // Creating the clustering info descriptor set layout, which is used for clustered rendering
    VkDescriptorSetLayoutBinding lightBufferBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0);
    VkDescriptorSetLayoutBinding clusteringInfoBufferBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 1);
    VkDescriptorSetLayoutBinding lightGridBufferBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 2);
    VkDescriptorSetLayoutBinding lightIndicesBufferBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 3);
    VkDescriptorSetLayoutBinding aabbBufferBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4);
    VkDescriptorSetLayoutBinding activeLightsBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5);

    std::array<VkDescriptorSetLayoutBinding, 6> clusteringBindings = {
        lightBufferBinding,        clusteringInfoBufferBinding, lightGridBufferBinding,
        lightIndicesBufferBinding, aabbBufferBinding,           activeLightsBinding};

    VkDescriptorSetLayoutCreateInfo clusterCreateInfo{};
    clusterCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    clusterCreateInfo.pNext        = nullptr;
    clusterCreateInfo.bindingCount = clusteringBindings.size();
    clusterCreateInfo.pBindings    = clusteringBindings.data();
    clusterCreateInfo.flags        = 0;

    if (vkCreateDescriptorSetLayout(mDevice, &clusterCreateInfo, nullptr, &mClusteringSetLayout) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Unable to create clustering descriptor set layout");
    }

    // Allocate the aabb buffer
    mAabbBuffer = createBuffer(sizeof(Aabb) * kNumClusters, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               VMA_MEMORY_USAGE_GPU_ONLY);

    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        constexpr int kMaxObjects = 10000;
        mFrames[i].objectBuffer =
            createBuffer(sizeof(GpuObjectData) * kMaxObjects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
        mFrames[i].cameraBuffer = createBuffer(
            sizeof(GpuCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        mFrames[i].dirLightBuffer = createBuffer(
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

        VkWriteDescriptorSet cameraSetWrite = init::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mFrames[i].globalDescriptor, &cameraBufferInfo, 0);

        // Use GpuObjectData for the second binding, which is an SSBO
        VkDescriptorBufferInfo objBufferInfo{};
        objBufferInfo.buffer = mFrames[i].objectBuffer.buffer;
        objBufferInfo.offset = 0;
        objBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet objSetWrite = init::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mFrames[i].globalDescriptor, &objBufferInfo, 1);

        // Use GpuDirLight for the third binding
        VkDescriptorBufferInfo dirLightBufferInfo{};
        dirLightBufferInfo.buffer = mFrames[i].dirLightBuffer.buffer;
        dirLightBufferInfo.offset = 0;
        dirLightBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet dirLightSetWrite = init::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mFrames[i].globalDescriptor, &dirLightBufferInfo, 2);

        // Allocate the clustering info descriptor set
        allocInfo.pSetLayouts = &mClusteringSetLayout;
        vkAllocateDescriptorSets(mDevice, &allocInfo, &mFrames[i].clusteringDescriptor);

        // Allocate the clustering info buffers
        size_t kLightBufferSize = sizeof(GpuPointLight) * mScene.pointLights.size();
        mFrames[i].lightBuffer  = createBuffer(kLightBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                               VMA_MEMORY_USAGE_CPU_TO_GPU);
        static constexpr size_t kClusteringInfoBufferSize = sizeof(ClusteringInfo);
        mFrames[i].clusteringInfoBuffer =
            createBuffer(kClusteringInfoBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
        static constexpr size_t kLightGridBufferSize = sizeof(LightGridEntry) * kNumClusters;
        mFrames[i].lightGridBuffer                   = createBuffer(
                              kLightGridBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        static constexpr size_t kLightIndicesBufferSize =
            sizeof(uint32_t) * kNumClusters * kMaxLightsPerCluster;
        mFrames[i].lightIndicesBuffer =
            createBuffer(kLightIndicesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
        static constexpr size_t kActiveLightsBufferSize = sizeof(uint32_t);
        mFrames[i].activeLightsBuffer =
            createBuffer(kActiveLightsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Write the light buffer info
        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.buffer = mFrames[i].lightBuffer.buffer;
        lightBufferInfo.offset = 0;
        lightBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet lightSetWrite =
            init::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        mFrames[i].clusteringDescriptor, &lightBufferInfo, 0);

        VkDescriptorBufferInfo clusteringBufferInfo;
        clusteringBufferInfo.buffer = mFrames[i].clusteringInfoBuffer.buffer;
        clusteringBufferInfo.offset = 0;
        clusteringBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet clusteringSetWrite =
            init::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        mFrames[i].clusteringDescriptor, &clusteringBufferInfo, 1);

        VkDescriptorBufferInfo lightGridBufferInfo{};
        lightGridBufferInfo.buffer = mFrames[i].lightGridBuffer.buffer;
        lightGridBufferInfo.offset = 0;
        lightGridBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet lightGridSetWrite =
            init::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        mFrames[i].clusteringDescriptor, &lightGridBufferInfo, 2);

        VkDescriptorBufferInfo lightIndicesBufferInfo{};
        lightIndicesBufferInfo.buffer = mFrames[i].lightIndicesBuffer.buffer;
        lightIndicesBufferInfo.offset = 0;
        lightIndicesBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet lightIndicesSetWrite = init::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mFrames[i].clusteringDescriptor,
            &lightIndicesBufferInfo, 3);

        VkDescriptorBufferInfo aabbBufferInfo{};
        aabbBufferInfo.buffer = mAabbBuffer.buffer;
        aabbBufferInfo.offset = 0;
        aabbBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet aabbBufferSetWrite = init::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mFrames[i].clusteringDescriptor, &aabbBufferInfo, 4);

        VkDescriptorBufferInfo activeLightsBufferInfo{};
        activeLightsBufferInfo.buffer = mFrames[i].activeLightsBuffer.buffer;
        activeLightsBufferInfo.offset = 0;
        activeLightsBufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet activeLightsSetWrite = init::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mFrames[i].clusteringDescriptor,
            &activeLightsBufferInfo, 5);

        std::array<VkWriteDescriptorSet, 9> writes = {
            cameraSetWrite,       objSetWrite,        dirLightSetWrite,
            lightSetWrite,        clusteringSetWrite, lightGridSetWrite,
            lightIndicesSetWrite, aabbBufferSetWrite, activeLightsSetWrite};

        vkUpdateDescriptorSets(mDevice, writes.size(), writes.data(), 0, nullptr);
    }

    VkDescriptorSetLayoutBinding albedoBinding = init::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    VkDescriptorSetLayoutBinding metallicRoughnessBinding = init::descriptorSetLayoutBinding(
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

    mDeleters.emplace_back([this]() {
        vkDestroyDescriptorSetLayout(mDevice, mGlobalSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mTextureSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mClusteringSetLayout, nullptr);
        vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

        vmaDestroyBuffer(mAllocator, mAabbBuffer.buffer, mAabbBuffer.allocation);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            vmaDestroyBuffer(mAllocator, mFrames[i].objectBuffer.buffer,
                             mFrames[i].objectBuffer.allocation);
            vmaDestroyBuffer(mAllocator, mFrames[i].cameraBuffer.buffer,
                             mFrames[i].cameraBuffer.allocation);
            vmaDestroyBuffer(mAllocator, mFrames[i].dirLightBuffer.buffer,
                             mFrames[i].dirLightBuffer.allocation);
            vmaDestroyBuffer(mAllocator, mFrames[i].lightBuffer.buffer,
                             mFrames[i].lightBuffer.allocation);
            vmaDestroyBuffer(mAllocator, mFrames[i].clusteringInfoBuffer.buffer,
                             mFrames[i].clusteringInfoBuffer.allocation);
            vmaDestroyBuffer(mAllocator, mFrames[i].lightGridBuffer.buffer,
                             mFrames[i].lightGridBuffer.allocation);
            vmaDestroyBuffer(mAllocator, mFrames[i].lightIndicesBuffer.buffer,
                             mFrames[i].lightIndicesBuffer.allocation);
            vmaDestroyBuffer(mAllocator, mFrames[i].activeLightsBuffer.buffer,
                             mFrames[i].activeLightsBuffer.allocation);
        }
    });
}

void HatGpuRenderer::createGraphicsPipeline()
{
    const auto vertShaderCode = readFile("../shaders/bin/shader.vert.spv");
    const auto fragShaderCode = readFile("../shaders/bin/shader.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertStageCreateInfo =
        init::pipelineShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule);
    VkPipelineShaderStageCreateInfo fragStageCreateInfo =
        init::pipelineShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertStageCreateInfo,
                                                                   fragStageCreateInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = init::vertexInputInfo();

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions    = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly =
        init::inputAssemblyInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

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

    VkPipelineRasterizationStateCreateInfo rasterizer =
        init::rasterizationInfo(VK_POLYGON_MODE_FILL);
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;

    VkPipelineMultisampleStateCreateInfo multisampling = init::multisampleInfo();

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo =
        init::pipelineDepthStencilInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = init::colorBlendAttachmentState();
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

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = init::pipelineLayoutInfo();
    pipelineLayoutCreateInfo.pushConstantRangeCount     = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges        = nullptr;
    std::array<VkDescriptorSetLayout, 3> layouts        = {mGlobalSetLayout, mClusteringSetLayout,
                                                           mTextureSetLayout};
    pipelineLayoutCreateInfo.setLayoutCount             = layouts.size();
    pipelineLayoutCreateInfo.pSetLayouts                = layouts.data();

    if (vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, nullptr,
                               &mGraphicsPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout object");
    }

    mDeleters.emplace_back(
        [this]() { vkDestroyPipelineLayout(mDevice, mGraphicsPipelineLayout, nullptr); });

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

    if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &mGraphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    mDeleters.emplace_back([this]() { vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr); });

    vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
}

void HatGpuRenderer::createComputePipelines()
{
    const auto aabbShader     = readFile("../shaders/bin/compute/aabb_gen.comp.spv");
    VkShaderModule aabbModule = createShaderModule(aabbShader);

    VkPipelineShaderStageCreateInfo aabbStageInfo =
        init::pipelineShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, aabbModule);

    VkPipelineLayoutCreateInfo aabbLayoutInfo = init::pipelineLayoutInfo();
    aabbLayoutInfo.pushConstantRangeCount     = 0;
    aabbLayoutInfo.pPushConstantRanges        = nullptr;
    aabbLayoutInfo.setLayoutCount             = 1;
    aabbLayoutInfo.pSetLayouts                = &mClusteringSetLayout;

    if (vkCreatePipelineLayout(mDevice, &aabbLayoutInfo, nullptr, &mAabbPipelineLayout) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create AABB pipeline layout");
    }

    mDeleters.emplace_back(
        [this]() { vkDestroyPipelineLayout(mDevice, mAabbPipelineLayout, nullptr); });

    VkComputePipelineCreateInfo aabbPipelineInfo{};
    aabbPipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    aabbPipelineInfo.pNext  = nullptr;
    aabbPipelineInfo.layout = mAabbPipelineLayout;
    aabbPipelineInfo.stage  = aabbStageInfo;

    if (vkCreateComputePipelines(mDevice, VK_NULL_HANDLE, 1, &aabbPipelineInfo, nullptr,
                                 &mAabbPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create AABB compute pipeline");
    }

    mDeleters.emplace_back([this]() { vkDestroyPipeline(mDevice, mAabbPipeline, nullptr); });

    vkDestroyShaderModule(mDevice, aabbModule, nullptr);

    const auto cullLightsShader     = readFile("../shaders/bin/compute/cull_lights.comp.spv");
    VkShaderModule cullLightsModule = createShaderModule(cullLightsShader);

    VkPipelineShaderStageCreateInfo cullLightsStageInfo =
        init::pipelineShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, cullLightsModule);

    VkPipelineLayoutCreateInfo cullLightsLayoutInfo           = init::pipelineLayoutInfo();
    cullLightsLayoutInfo.pushConstantRangeCount               = 0;
    cullLightsLayoutInfo.pPushConstantRanges                  = nullptr;
    std::array<VkDescriptorSetLayout, 2> cullLightsSetLayouts = {mGlobalSetLayout,
                                                                 mClusteringSetLayout};
    cullLightsLayoutInfo.setLayoutCount                       = cullLightsSetLayouts.size();
    cullLightsLayoutInfo.pSetLayouts                          = cullLightsSetLayouts.data();

    if (vkCreatePipelineLayout(mDevice, &cullLightsLayoutInfo, nullptr, &mLightCullLayout) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create layout for light culling pipeline");
    }

    mDeleters.emplace_back(
        [this]() { vkDestroyPipelineLayout(mDevice, mLightCullLayout, nullptr); });

    VkComputePipelineCreateInfo cullLightsPipelineInfo{};
    cullLightsPipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cullLightsPipelineInfo.pNext  = nullptr;
    cullLightsPipelineInfo.layout = mLightCullLayout;
    cullLightsPipelineInfo.stage  = cullLightsStageInfo;

    if (vkCreateComputePipelines(mDevice, VK_NULL_HANDLE, 1, &cullLightsPipelineInfo, nullptr,
                                 &mLightCullPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create light culling pipeline");
    }

    mDeleters.emplace_back([this]() { vkDestroyPipeline(mDevice, mLightCullPipeline, nullptr); });

    vkDestroyShaderModule(mDevice, cullLightsModule, nullptr);
}

void HatGpuRenderer::createUploadContext()
{
    VkFenceCreateInfo uploadFenceCreateInfo = init::fenceInfo();
    if (vkCreateFence(mDevice, &uploadFenceCreateInfo, nullptr, &mUploadContext.uploadFence) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create upload context fence");
    }

    mDeleters.emplace_back(
        [this]() { vkDestroyFence(mDevice, mUploadContext.uploadFence, nullptr); });

    VkCommandPoolCreateInfo commandPoolCreateInfo = init::commandPoolInfo(mGraphicsQueueIndex);
    if (vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr,
                            &mUploadContext.commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create upload context command pool");
    }

    mDeleters.emplace_back(
        [this]() { vkDestroyCommandPool(mDevice, mUploadContext.commandPool, nullptr); });

    VkCommandBufferAllocateInfo commandBufferAllocInfo =
        init::commandBufferAllocInfo(mUploadContext.commandPool);
    if (vkAllocateCommandBuffers(mDevice, &commandBufferAllocInfo, &mUploadContext.commandBuffer) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate upload context command buffer");
    }
}

void HatGpuRenderer::immediateSubmit(std::function<void(VkCommandBuffer)> &&function)
{
    VkCommandBuffer cmd = mUploadContext.commandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo =
        init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    if (vkBeginCommandBuffer(cmd, &cmdBeginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin command buffer");
    }

    function(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to end command buffer");
    }

    VkSubmitInfo submitInfo = init::submitInfo(&cmd);
    if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mUploadContext.uploadFence) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit to queue");
    }

    vkWaitForFences(mDevice, 1, &mUploadContext.uploadFence, true,
                    std::numeric_limits<uint32_t>::max());
    vkResetFences(mDevice, 1, &mUploadContext.uploadFence);

    vkResetCommandPool(mDevice, mUploadContext.commandPool, 0);
}

void HatGpuRenderer::createRenderPass()
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

AllocatedBuffer HatGpuRenderer::createBuffer(size_t allocSize,
                                             VkBufferUsageFlags usage,
                                             VmaMemoryUsage memoryUsage)
{
    // allocate vertex buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;

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

void HatGpuRenderer::uploadMesh(Mesh &mesh)
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

void HatGpuRenderer::uploadTextures(Mesh &mesh)
{
    // Check whether linear GPU blitting (required by mipmaps) is even supported by this hardware
    // before we do all sorts of work
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, VK_FORMAT_R8G8B8A8_SRGB,
                                        &formatProperties);
    if (!(formatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("Texture image format does not support linear blitting");
    }

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
        VkDeviceSize imageSize = cpuTexture->width * cpuTexture->height * 4;
        AllocatedBuffer stagingBuffer =
            createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        // Write the CPU texture data into the staging buffer
        void *data;
        vmaMapMemory(mAllocator, stagingBuffer.allocation, &data);
        std::memcpy(data, cpuTexture->pixels, imageSize);
        vmaUnmapMemory(mAllocator, stagingBuffer.allocation);

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
            init::imageInfo(VK_FORMAT_R8G8B8A8_SRGB,
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            imageExtent, mipLevels);
        AllocatedImage newImage;
        VmaAllocationCreateInfo imgAllocInfo{};
        imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(mAllocator, &imageInfo, &imgAllocInfo, &newImage.image, &newImage.allocation,
                       nullptr);

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
        vmaDestroyBuffer(mAllocator, stagingBuffer.buffer, stagingBuffer.allocation);
        mDeleters.emplace_back([this, newImage]() {
            vmaDestroyImage(mAllocator, newImage.image, newImage.allocation);
        });

        // Create the image view
        VkImageViewCreateInfo imageViewInfo = init::imageViewInfo(
            VK_FORMAT_R8G8B8A8_SRGB, newImage.image, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
        vkCreateImageView(mDevice, &imageViewInfo, nullptr, &dstTexture.imageView);
    }

    for (const auto &[typ, path] : mesh.textures)
    {
        VkSamplerCreateInfo samplerInfo = init::samplerInfo(VK_FILTER_LINEAR);
        samplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.maxLod              = mGpuTextures[path].mipLevels;
        samplerInfo.anisotropyEnable    = VK_TRUE;
        samplerInfo.mipLodBias          = -0.5f;
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        VkSampler sampler;
        vkCreateSampler(mDevice, &samplerInfo, nullptr, &sampler);
        mDeleters.emplace_back([this, sampler]() { vkDestroySampler(mDevice, sampler, nullptr); });

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
            init::writeDescriptorImage(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mesh.descriptor,
                                       &imageBufferInfo, dstBinding);
        vkUpdateDescriptorSets(mDevice, 1, &descriptorWrite, 0, nullptr);
    }
}

void HatGpuRenderer::loadSceneFromDisk()
{
    mScene.loadFromJson(mScenePath, mTextureManager);
}

void HatGpuRenderer::uploadSceneToGpu()
{
    for (auto &renderable : mScene.renderables)
    {
        for (auto &mesh : renderable.model->meshes)
        {
            uploadMesh(mesh);
            uploadTextures(mesh);
        }
    }

    mDeleters.emplace_back([this]() {
        for (const auto &[_, texture] : mGpuTextures)
        {
            vkDestroyImageView(mDevice, texture.imageView, nullptr);
        }
    });

    void *data;
    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        vmaMapMemory(mAllocator, mFrames[i].clusteringInfoBuffer.allocation, &data);
        ClusteringInfo clusteringInfo{};
        clusteringInfo.projInverse      = glm::inverse(mCamera.GetProjectionMatrix());
        clusteringInfo.screenDimensions = glm::vec2(mCamera.ScreenWidth, mCamera.ScreenHeight);
        clusteringInfo.zNear            = mCamera.Near;
        clusteringInfo.zFar             = mCamera.Far;
        clusteringInfo.tileSizeX        = mCamera.ScreenWidth / kNumTilesX;
        clusteringInfo.tileSizeY        = mCamera.ScreenHeight / kNumTilesY;
        clusteringInfo.numZSlices       = kNumSlicesZ;
        clusteringInfo.scale =
            static_cast<float>(kNumSlicesZ) / glm::log2(mCamera.Far / mCamera.Near);
        clusteringInfo.bias = -(static_cast<float>(kNumSlicesZ) * glm::log2(mCamera.Near) /
                                glm::log2(mCamera.Far / mCamera.Near));
        std::memcpy(data, &clusteringInfo, sizeof(ClusteringInfo));

        vmaUnmapMemory(mAllocator, mFrames[i].clusteringInfoBuffer.allocation);
    }
}

void HatGpuRenderer::generateAabb()
{
    immediateSubmit([this](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mAabbPipeline);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mAabbPipelineLayout, 0, 1,
                                    &mFrames[i].clusteringDescriptor, 0, nullptr);
            vkCmdDispatch(cmd, kNumTilesX, kNumTilesY, kNumSlicesZ);
        }

        VkBufferMemoryBarrier barrierInfo{};
        barrierInfo.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrierInfo.pNext               = nullptr;
        barrierInfo.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        barrierInfo.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        barrierInfo.srcQueueFamilyIndex = mGraphicsQueueIndex;
        barrierInfo.dstQueueFamilyIndex = mGraphicsQueueIndex;
        barrierInfo.buffer              = mAabbBuffer.buffer;
        barrierInfo.offset              = 0;
        barrierInfo.size                = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrierInfo,
                             0, nullptr);
    });
}

void HatGpuRenderer::drawObjects(const VkCommandBuffer &commandBuffer)
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
        void *data;
        vmaMapMemory(mAllocator, mFrames[mCurrentFrameIndex].cameraBuffer.allocation, &data);
        std::memcpy(data, &cameraData, sizeof(GpuCameraData));
        vmaUnmapMemory(mAllocator, mFrames[mCurrentFrameIndex].cameraBuffer.allocation);

        // Writing all of the object transforms
        vmaMapMemory(mAllocator, mFrames[mCurrentFrameIndex].objectBuffer.allocation, &data);
        auto objectData = static_cast<GpuObjectData *>(data);
        for (size_t i = 0; i < mScene.renderables.size(); ++i)
        {
            objectData[i].modelTransform = mScene.renderables[i].transform;
        }
        vmaUnmapMemory(mAllocator, mFrames[mCurrentFrameIndex].objectBuffer.allocation);

        // Writing the dir light
        vmaMapMemory(mAllocator, mFrames[mCurrentFrameIndex].dirLightBuffer.allocation, &data);
        auto dirLightData       = static_cast<GpuDirLight *>(data);
        dirLightData->direction = glm::vec4(mScene.dirLight.direction, 0.f);
        dirLightData->color     = glm::vec4(mScene.dirLight.color, 0.f);
        vmaUnmapMemory(mAllocator, mFrames[mCurrentFrameIndex].dirLightBuffer.allocation);

        // Writing the lights to the light buffer
        vmaMapMemory(mAllocator, mFrames[mCurrentFrameIndex].lightBuffer.allocation, &data);
        auto lightBufferData = static_cast<GpuPointLight *>(data);
        for (size_t i = 0; i < mScene.pointLights.size(); ++i)
        {
            lightBufferData[i].position = glm::vec4(mScene.pointLights[i].position, 0.f);
            lightBufferData[i].color    = glm::vec4(mScene.pointLights[i].color, 0.f);
        }
        vmaUnmapMemory(mAllocator, mFrames[mCurrentFrameIndex].lightBuffer.allocation);
    }

    cullLights(commandBuffer, mCurrentFrameIndex);

    VkRenderPassBeginInfo renderPassInfo = init::renderPassBeginInfo(
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

        std::array<VkDescriptorSet, 2> descriptorSets = {
            mFrames[mCurrentFrameIndex].globalDescriptor,
            mFrames[mCurrentFrameIndex].clusteringDescriptor};
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
                                    mGraphicsPipelineLayout, 2, 1, &mesh.descriptor, 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(commandBuffer);
}

void HatGpuRenderer::cullLights(VkCommandBuffer cmd, uint32_t imageIndex)
{
    ZoneScopedC(tracy::Color::Cornsilk);
    TracyVkZoneC(mCurrentApplicationFrame->tracyContext, cmd, "Light culling",
                 tracy::Color::DarkViolet);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mLightCullPipeline);

    void *data;
    vmaMapMemory(mAllocator, mFrames[imageIndex].activeLightsBuffer.allocation, &data);
    auto counterPtr = static_cast<uint32_t *>(data);
    *counterPtr     = 0;
    vmaUnmapMemory(mAllocator, mFrames[imageIndex].activeLightsBuffer.allocation);

    std::array<VkDescriptorSet, 2> descriptorSets = {mFrames[imageIndex].globalDescriptor,
                                                     mFrames[imageIndex].clusteringDescriptor};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mLightCullLayout, 0,
                            descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    vkCmdDispatch(cmd, kNumTilesX, kNumTilesY, kNumSlicesZ);

    VkBufferMemoryBarrier lightGridBarrier;
    lightGridBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    lightGridBarrier.pNext               = nullptr;
    lightGridBarrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    lightGridBarrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    lightGridBarrier.srcQueueFamilyIndex = mGraphicsQueueIndex;
    lightGridBarrier.dstQueueFamilyIndex = mGraphicsQueueIndex;
    lightGridBarrier.buffer              = mFrames[imageIndex].lightGridBuffer.buffer;
    lightGridBarrier.offset              = 0;
    lightGridBarrier.size                = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier lightIndicesBarrier;
    lightIndicesBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    lightIndicesBarrier.pNext               = nullptr;
    lightIndicesBarrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    lightIndicesBarrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    lightIndicesBarrier.srcQueueFamilyIndex = mGraphicsQueueIndex;
    lightIndicesBarrier.dstQueueFamilyIndex = mGraphicsQueueIndex;
    lightIndicesBarrier.buffer              = mFrames[imageIndex].lightIndicesBuffer.buffer;
    lightIndicesBarrier.offset              = 0;
    lightIndicesBarrier.size                = VK_WHOLE_SIZE;

    std::array<VkBufferMemoryBarrier, 2> barriers = {lightGridBarrier, lightIndicesBarrier};

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, barriers.size(),
                         barriers.data(), 0, nullptr);
}

void HatGpuRenderer::recordCommandBuffer(const VkCommandBuffer &commandBuffer,
                                         [[maybe_unused]] uint32_t imageIndex)
{
    ZoneScopedC(tracy::Color::PeachPuff);

    drawObjects(commandBuffer);
}

HatGpuRenderer::~HatGpuRenderer()
{
    vkDeviceWaitIdle(mDevice);

    while (!mDeleters.empty())
    {
        Deleter nextDeleter = mDeleters.back();
        nextDeleter();
        mDeleters.pop_back();
    }
}

}  // namespace hatgpu
