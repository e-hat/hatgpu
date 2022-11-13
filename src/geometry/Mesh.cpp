#include "efpch.h"

#include "Mesh.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <glm/gtx/string_cast.hpp>

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

            v.color = normal;

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
        std::cerr << "Failed to load mesh from '" << filename << "'\n";
        return false;
    }

    processNode(scene->mRootNode, scene, vertices);
    return true;
}
}  // namespace efvk
