#include "hatpch.h"

#include "Mesh.h"

#include "vk/allocator.h"
#include "vk/types.h"
#include "vk/upload_context.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>

namespace hatgpu
{
namespace
{

void processNode(aiNode *node, const aiScene *scene, std::vector<Vertex> &vertices)
{
    for (size_t i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

        for (size_t j = 0; j < mesh->mNumVertices; ++j)
        {
            Vertex v;

            glm::vec3 &position = v.position;
            position.x          = mesh->mVertices[j].x;
            position.y          = mesh->mVertices[j].y;
            position.z          = mesh->mVertices[j].z;

            glm::vec3 &normal = v.normal;
            normal.x          = mesh->mNormals[j].x;
            normal.y          = mesh->mNormals[j].y;
            normal.z          = mesh->mNormals[j].z;

            vertices.push_back(v);
        }
    }

    for (size_t i = 0; i < node->mNumChildren; ++i)
    {
        processNode(node->mChildren[i], scene, vertices);
    }
}
}  // namespace

void TextureManager::loadTexture(const std::string &file)
{
    if (textures.contains(file))
    {
        return;
    }

    int width, height, channels;
    stbi_uc *ucPixels = stbi_load(file.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    H_ASSERT(ucPixels != nullptr, std::string("Failed to load texture file: ") + file);

    auto result    = std::make_shared<Texture>(static_cast<void *>(ucPixels), width, height);
    textures[file] = std::move(result);
}

Texture::~Texture()
{
    stbi_image_free(pixels);
}

bool Mesh::loadFromObj(const std::string &filename)
{
    Assimp::Importer importer;
    const aiScene *scene =
        importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs |
                                        aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph);

    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        scene->mRootNode == nullptr)
    {
        std::cerr << "Failed to load mesh from '" << filename << "'\n";
        return false;
    }

    processNode(scene->mRootNode, scene, vertices);
    return true;
}

void Mesh::upload(vk::Allocator &allocator, vk::UploadContext &context)
{
    // Uploading the vertex data followed by the index data in contiguous memory
    const size_t verticesSize = vertices.size() * sizeof(Vertex);
    const size_t indicesSize  = indices.size() * sizeof(Mesh::IndexType);
    const size_t bufferSize   = verticesSize + indicesSize;

    vk::AllocatedBuffer stagingBuffer = allocator.createBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void *data = allocator.map(stagingBuffer);
    std::memcpy(data, vertices.data(), verticesSize);
    std::memcpy(static_cast<char *>(data) + verticesSize, indices.data(), indicesSize);
    allocator.unmap(stagingBuffer);

    vertexBuffer = allocator.createBuffer(
        verticesSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    indexBuffer = allocator.createBuffer(
        verticesSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    context.immediateSubmit([=, this](VkCommandBuffer cmd) {
        VkBufferCopy vboCopy{};
        vboCopy.dstOffset = 0;
        vboCopy.srcOffset = 0;
        vboCopy.size      = verticesSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, vertexBuffer.buffer, 1, &vboCopy);

        VkBufferCopy iboCopy{};
        iboCopy.dstOffset = 0;
        iboCopy.srcOffset = verticesSize;
        iboCopy.size      = indicesSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, indexBuffer.buffer, 1, &iboCopy);
    });

    allocator.destroyBuffer(stagingBuffer);
}

void Mesh::destroyBuffers(vk::Allocator &allocator)
{
    allocator.destroyBuffer(vertexBuffer);
    allocator.destroyBuffer(indexBuffer);
}
}  // namespace hatgpu
