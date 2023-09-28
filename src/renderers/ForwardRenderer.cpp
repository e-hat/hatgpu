#include <vulkan/vulkan_core.h>
#include "application/Layer.h"
#include "hatpch.h"

#include "ForwardRenderer.h"
#include "imgui.h"
#include "texture/Texture.h"
#include "util/Random.h"
#include "vk/initializers.h"
#include "vk/shader.h"

#include <tracy/Tracy.hpp>

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

static constexpr const char *kVertexShaderName   = "../shaders/bin/forward/shader.vert.spv";
static constexpr const char *kFragmentShaderName = "../shaders/bin/forward/shader.frag.spv";
}  // namespace

ForwardRenderer::ForwardRenderer(std::shared_ptr<vk::Ctx> ctx, std::shared_ptr<Scene> scene)
    : Layer("ForwardRenderer", ctx, scene)
{}

const LayerRequirements ForwardRenderer::kRequirements = {
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME}};

void ForwardRenderer::OnAttach()
{
    if (mInitialized)
    {
        return;
    }

    createDescriptors();
    createGraphicsPipeline();
    uploadSceneToGpu();

    mInitialized = true;
}

void ForwardRenderer::OnDetach() {}

void ForwardRenderer::OnRender(DrawCtx &drawCtx)
{
    recordCommandBuffer(drawCtx);
    ++mFrameCount;
}

void ForwardRenderer::OnImGuiRender()
{
    ImGui::Text("hi from forward renderer :3");
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

    vkCreateDescriptorPool(mCtx->device, &poolInfo, nullptr, &mDescriptorPool);

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

    H_CHECK(
        vkCreateDescriptorSetLayout(mCtx->device, &globalCreateInfo, nullptr, &mGlobalSetLayout),
        "Unable to create global descriptor set layout");

    for (size_t i = 0; i < constants::kMaxFramesInFlight; ++i)
    {
        constexpr int kMaxObjects = 10000;
        mFrames[i].objectBuffer = mCtx->allocator.createBuffer(sizeof(GpuObjectData) * kMaxObjects,
                                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                               VMA_MEMORY_USAGE_CPU_TO_GPU);
        mFrames[i].cameraBuffer = mCtx->allocator.createBuffer(
            sizeof(GpuCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        mFrames[i].dirLightBuffer = mCtx->allocator.createBuffer(
            sizeof(GpuDirLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Create this descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.pNext              = nullptr;
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &mGlobalSetLayout;

        vkAllocateDescriptorSets(mCtx->device, &allocInfo, &mFrames[i].globalDescriptor);

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
            sizeof(GpuPointLight) * std::max(static_cast<size_t>(1), mScene->pointLights.size());
        mFrames[i].lightBuffer = mCtx->allocator.createBuffer(
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

        vkUpdateDescriptorSets(mCtx->device, writes.size(), writes.data(), 0, nullptr);
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

    vkCreateDescriptorSetLayout(mCtx->device, &textureLayoutInfo, nullptr, &mTextureSetLayout);

    mDeleter.enqueue([this]() {
        H_LOG("...destroying descriptor sets");
        vkDestroyDescriptorSetLayout(mCtx->device, mGlobalSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mCtx->device, mTextureSetLayout, nullptr);
        vkDestroyDescriptorPool(mCtx->device, mDescriptorPool, nullptr);

        H_LOG("...destroying buffers");
        for (size_t i = 0; i < constants::kMaxFramesInFlight; ++i)
        {
            mCtx->allocator.destroyBuffer(mFrames[i].objectBuffer);
            mCtx->allocator.destroyBuffer(mFrames[i].cameraBuffer);
            mCtx->allocator.destroyBuffer(mFrames[i].dirLightBuffer);
            mCtx->allocator.destroyBuffer(mFrames[i].lightBuffer);
        }
    });
}

void ForwardRenderer::createGraphicsPipeline()
{
    H_LOG("...creating graphics pipeline");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        vk::createShaderStage(mCtx->device, kVertexShaderName, VK_SHADER_STAGE_VERTEX_BIT),
        vk::createShaderStage(mCtx->device, kFragmentShaderName, VK_SHADER_STAGE_FRAGMENT_BIT),
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
    viewport.width    = static_cast<float>(mCtx->swapchainExtent.width);
    viewport.height   = static_cast<float>(mCtx->swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mCtx->swapchainExtent;

    auto viewportState = vk::viewportStateInfo(&viewport, &scissor);

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

    H_CHECK(vkCreatePipelineLayout(mCtx->device, &pipelineLayoutCreateInfo, nullptr,
                                   &mGraphicsPipelineLayout),
            "Failed to create pipeline layout object");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying graphics pipeline layout");
        vkDestroyPipelineLayout(mCtx->device, mGraphicsPipelineLayout, nullptr);
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
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass    = 0;

    VkPipelineRenderingCreateInfo pipelineCreateRenderingInfo{};
    pipelineCreateRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineCreateRenderingInfo.pNext = nullptr;
    pipelineCreateRenderingInfo.colorAttachmentCount    = 1;
    pipelineCreateRenderingInfo.pColorAttachmentFormats = &mCtx->swapchainImageFormat;
    pipelineCreateRenderingInfo.depthAttachmentFormat   = constants::kDepthFormat;

    pipelineInfo.pNext = &pipelineCreateRenderingInfo;

    H_CHECK(vkCreateGraphicsPipelines(mCtx->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &mGraphicsPipeline),
            "Failed to create graphics pipeline");

    mDeleter.enqueue([this]() {
        H_LOG("...destroying graphics pipeline");
        vkDestroyPipeline(mCtx->device, mGraphicsPipeline, nullptr);
    });

    for (const auto &stage : shaderStages)
    {
        vkDestroyShaderModule(mCtx->device, stage.module, nullptr);
    }
}

void ForwardRenderer::uploadTextures(Mesh &mesh)
{
    Texture::checkRequiredFormatProperties(mCtx->physicalDevice);

    // Each mesh has its own descriptor set allocated from the global pool
    VkDescriptorSetAllocateInfo textureDescriptorInfo{};
    textureDescriptorInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    textureDescriptorInfo.pNext              = nullptr;
    textureDescriptorInfo.descriptorPool     = mDescriptorPool;
    textureDescriptorInfo.descriptorSetCount = 1;
    textureDescriptorInfo.pSetLayouts        = &mTextureSetLayout;
    vkAllocateDescriptorSets(mCtx->device, &textureDescriptorInfo, &mesh.descriptor);

    // We go through all of a mesh's textures and upload them to the GPU
    for (const auto &[typ, path] : mesh.textures)
    {
        // If we've already uploaded this mesh's texture we can skip it
        if (mGpuTextures.contains(path))
            continue;

        std::shared_ptr<Texture> cpuTexture = mScene->textureManager.textures[path];
        vk::GpuTexture gpuTexture =
            cpuTexture->upload(mCtx->device, mCtx->allocator, mCtx->uploadContext);
        mGpuTextures[path] = gpuTexture;

        // Get rid of the staging buffer, queue the deletion operation for the GPU image
        mDeleter.enqueue([this, gpuTexture]() mutable { gpuTexture.destroy(mCtx->allocator); });
    }

    for (const auto &[typ, path] : mesh.textures)
    {
        VkSamplerCreateInfo samplerInfo = vk::samplerInfo(VK_FILTER_LINEAR);
        samplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.maxLod              = mGpuTextures[path].mipLevels;
        samplerInfo.anisotropyEnable    = VK_TRUE;
        samplerInfo.mipLodBias          = -0.5f;
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(mCtx->physicalDevice, &properties);
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        VkSampler sampler;
        vkCreateSampler(mCtx->device, &samplerInfo, nullptr, &sampler);
        mDeleter.enqueue([this, sampler]() { vkDestroySampler(mCtx->device, sampler, nullptr); });

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
        vkUpdateDescriptorSets(mCtx->device, 1, &descriptorWrite, 0, nullptr);
    }
}

void ForwardRenderer::uploadSceneToGpu()
{
    H_LOG("...uploading scene to GPU");
    for (auto &renderable : mScene->renderables)
    {
        for (auto &mesh : renderable.model->meshes)
        {
            mesh.upload(mCtx->allocator, mCtx->uploadContext);
            mDeleter.enqueue([this, mesh]() mutable { mesh.destroyBuffers(mCtx->allocator); });

            uploadTextures(mesh);
        }
    }

    mDeleter.enqueue([this]() {
        H_LOG("...destroying texture image views");
        for (const auto &[_, texture] : mGpuTextures)
        {
            vkDestroyImageView(mCtx->device, texture.imageView, nullptr);
        }
    });
}

void ForwardRenderer::drawObjects(DrawCtx &drawCtx)
{
    VkZoneC("drawObjects", tracy::Color::Blue);
    const glm::mat4 view     = mScene->camera.GetViewMatrix();
    const glm::mat4 proj     = mScene->camera.GetProjectionMatrix();
    const glm::mat4 viewproj = proj * view;

    GpuCameraData cameraData;
    cameraData.proj     = proj;
    cameraData.view     = view;
    cameraData.viewproj = viewproj;
    cameraData.position = mScene->camera.Position;

    {
        ZoneScopedNC("Scene buffer writes", tracy::Color::DeepSkyBlue4);
        VkZoneC("Scene buffer writes", tracy::Color::Olive);
        // Writing camera data
        void *data = mCtx->allocator.map(mFrames[drawCtx.frameIndex].cameraBuffer);
        std::memcpy(data, &cameraData, sizeof(GpuCameraData));
        mCtx->allocator.unmap(mFrames[drawCtx.frameIndex].cameraBuffer);

        // Writing all of the object transforms
        data            = mCtx->allocator.map(mFrames[drawCtx.frameIndex].objectBuffer);
        auto objectData = static_cast<GpuObjectData *>(data);
        for (size_t i = 0; i < mScene->renderables.size(); ++i)
        {
            objectData[i].modelTransform = mScene->renderables[i].transform;
        }
        mCtx->allocator.unmap(mFrames[drawCtx.frameIndex].objectBuffer);

        // Writing the dir light
        data                    = mCtx->allocator.map(mFrames[drawCtx.frameIndex].dirLightBuffer);
        auto dirLightData       = static_cast<GpuDirLight *>(data);
        dirLightData->direction = glm::vec4(mScene->dirLight.direction, 0.f);
        dirLightData->color     = glm::vec4(mScene->dirLight.color, 0.f);
        mCtx->allocator.unmap(mFrames[drawCtx.frameIndex].dirLightBuffer);

        // Writing the lights to the light buffer
        data                 = mCtx->allocator.map(mFrames[drawCtx.frameIndex].lightBuffer);
        auto lightBufferData = static_cast<GpuPointLight *>(data);
        for (size_t i = 0; i < mScene->pointLights.size(); ++i)
        {
            lightBufferData[i].position = glm::vec4(mScene->pointLights[i].position, 0.f);
            lightBufferData[i].color    = glm::vec4(mScene->pointLights[i].color, 0.f);
        }
        mCtx->allocator.unmap(mFrames[drawCtx.frameIndex].lightBuffer);
    }

    {
        VkZoneC("Renderpass Begin", tracy::Color::LavenderBlush);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView   = drawCtx.swapchainImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue  = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView   = drawCtx.depthImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        VkClearValue depthClear{};
        depthClear.depthStencil.depth = 1.0f;
        depthAttachment.clearValue    = depthClear;

        VkRenderingInfo renderInfo{};
        renderInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderInfo.flags                = 0;
        renderInfo.renderArea           = {.offset = {0, 0}, .extent = mCtx->swapchainExtent};
        renderInfo.layerCount           = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments    = &colorAttachment;
        renderInfo.pDepthAttachment     = &depthAttachment;

        vkCmdBeginRendering(drawCtx.commandBuffer, &renderInfo);

        vkCmdBindPipeline(drawCtx.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mGraphicsPipeline);

        std::array<VkDescriptorSet, 1> descriptorSets = {
            mFrames[drawCtx.frameIndex].globalDescriptor};
        vkCmdBindDescriptorSets(drawCtx.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mGraphicsPipelineLayout, 0, descriptorSets.size(),
                                descriptorSets.data(), 0, nullptr);

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

    for (const auto &object : mScene->renderables)
    {
        ZoneScopedC(tracy::Color::AntiqueWhite);

        for (auto &mesh : object.model->meshes)
        {
            ZoneScopedC(tracy::Color::DodgerBlue);
            VkZoneC("Mesh Draw", tracy::Color::Red);
            if (!mesh.textures.contains(TextureType::ALBEDO) ||
                !mesh.textures.contains(TextureType::METALLIC_ROUGHNESS))
                continue;

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(drawCtx.commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &offset);
            vkCmdBindIndexBuffer(drawCtx.commandBuffer, mesh.indexBuffer.buffer, 0,
                                 VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(drawCtx.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mGraphicsPipelineLayout, 1, 1, &mesh.descriptor, 0, nullptr);

            vkCmdDrawIndexed(drawCtx.commandBuffer, static_cast<uint32_t>(mesh.indices.size()), 1,
                             0, 0, 0);
        }
    }

    vkCmdEndRendering(drawCtx.commandBuffer);
}

void ForwardRenderer::recordCommandBuffer(DrawCtx &drawCtx)
{
    ZoneScopedC(tracy::Color::PeachPuff);

    drawObjects(drawCtx);
}

}  // namespace hatgpu
