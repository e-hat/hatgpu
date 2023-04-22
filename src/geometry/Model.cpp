#include "Model.h"
#include "hatpch.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <glm/gtx/string_cast.hpp>

#include <iostream>

namespace hatgpu
{
namespace
{
std::vector<std::string> loadMaterialTextures(aiMaterial *mat,
                                              aiTextureType typ,
                                              TextureManager &manager,
                                              const std::string &dir)
{
    size_t numTextures = mat->GetTextureCount(typ);
    std::vector<std::string> texturePaths;
    texturePaths.reserve(numTextures);
    for (size_t i = 0; i < numTextures; ++i)
    {
        aiString str;
        mat->GetTexture(typ, i, &str);
        const std::string path = dir + "/" + str.C_Str();
        manager.loadTexture(path);
        texturePaths.emplace_back(path);
    }

    return texturePaths;
}
}  // namespace

void Model::loadFromObj(const std::string &filename, TextureManager &manager)
{
    Assimp::Importer importer;
    const aiScene *scene =
        importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs |
                                        aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph |
                                        aiProcess_ForceGenNormals | aiProcess_FlipWindingOrder);

    H_ASSERT(scene != nullptr && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) &&
                 scene->mRootNode != nullptr,
             std::string("Failed to load mesh from ") + filename);

    // Group meshes by material
    meshes.resize(scene->mNumMaterials - 1);

    mDirectory = filename.substr(0, filename.find_last_of('/'));

    processNode(scene->mRootNode, scene, manager);
}

void Model::processNode(aiNode *node, const aiScene *scene, TextureManager &manager)
{
    for (size_t i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh *mesh             = scene->mMeshes[node->mMeshes[i]];
        size_t meshIndex         = mesh->mMaterialIndex;
        Mesh &efMesh             = meshes[meshIndex];
        size_t numVerticesBefore = efMesh.vertices.size();
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

            glm::vec2 &uv = v.uv;
            uv.x          = mesh->mTextureCoords[0][j].x;
            uv.y          = mesh->mTextureCoords[0][j].y;

            efMesh.vertices.push_back(v);
        }

        for (size_t j = 0; j < mesh->mNumFaces; ++j)
        {
            aiFace face = mesh->mFaces[j];
            for (size_t k = 0; k < face.mNumIndices; ++k)
            {
                efMesh.indices.push_back(face.mIndices[k] + numVerticesBefore);
            }
        }

        aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];

        auto albedos = loadMaterialTextures(mat, aiTextureType_BASE_COLOR, manager, mDirectory);
        if (!albedos.empty())
            efMesh.textures[TextureType::ALBEDO] = albedos.front();

        auto metalnesses = loadMaterialTextures(mat, aiTextureType_METALNESS, manager, mDirectory);
        if (!metalnesses.empty())
            efMesh.textures[TextureType::METALLIC_ROUGHNESS] = metalnesses.front();
        else
        {
            auto roughnesses =
                loadMaterialTextures(mat, aiTextureType_DIFFUSE_ROUGHNESS, manager, mDirectory);
            if (!roughnesses.empty())
                efMesh.textures[TextureType::METALLIC_ROUGHNESS] = roughnesses.front();
        }
    }
}
}  // namespace hatgpu
