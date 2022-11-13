#include "Model.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <iostream>

namespace efvk
{
void Model::loadFromObj(const std::string &filename)
{
    Assimp::Importer importer;
    const aiScene *scene =
        importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs |
                                        aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph);

    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        scene->mRootNode == nullptr)
    {
        throw std::runtime_error(std::string("Failed to load mesh from ") + filename);
    }

    // Group meshes by material
    meshes.resize(scene->mNumMaterials - 1);

    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode *node, const aiScene *scene)
{
    for (size_t i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh *mesh             = scene->mMeshes[node->mMeshes[i]];
        size_t meshIndex         = mesh->mMaterialIndex - 1;
        size_t numVerticesBefore = meshes[meshIndex].vertices.size();
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

            v.color = normal;

            meshes[meshIndex].vertices.push_back(v);
        }

        for (size_t j = 0; j < mesh->mNumFaces; ++j)
        {
            aiFace face = mesh->mFaces[j];
            for (size_t k = 0; k < face.mNumIndices; ++k)
            {
                meshes[meshIndex].indices.push_back(face.mIndices[k] + numVerticesBefore);
            }
        }
    }
}
}  // namespace efvk
