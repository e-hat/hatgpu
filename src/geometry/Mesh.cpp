#include "hatpch.h"

#include "Mesh.h"

#include "vk/allocator.h"
#include "vk/types.h"
#include "vk/upload_context.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

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

bool Mesh::loadFromObj(const std::string &filename)
{
    Assimp::Importer importer;
    const aiScene *scene =
        importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs |
                                        aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph);

    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        scene->mRootNode == nullptr)
    {
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
        indicesSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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

Aabb Mesh::BoundingBox(const glm::mat4 &worldTransform) const
{
    if (vertices.empty())
    {
        return Aabb{glm::vec4(0.f), glm::vec4(0.f)};
    }

    Aabb result{worldTransform * glm::vec4(vertices.front().position, 1.0f),
                worldTransform * glm::vec4(vertices.front().position, 1.0f)};
    for (const auto &vertex : vertices)
    {
        const glm::vec4 worldPos = worldTransform * glm::vec4(vertex.position, 1.0f);
        result.min               = glm::min(result.min, worldPos);
        result.max               = glm::max(result.max, worldPos);
    }

    return result;
}
}  // namespace hatgpu
