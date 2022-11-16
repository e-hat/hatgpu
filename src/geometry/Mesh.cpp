#include "efpch.h"

#include "Mesh.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>

namespace efvk
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

    if (ucPixels == nullptr)
    {
        throw std::runtime_error(std::string("Failed to load texture file: ") + file);
    }

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
}  // namespace efvk
