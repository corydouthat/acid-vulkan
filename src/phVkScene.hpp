// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Main Engine Class

#pragma once

#include <iostream>
#include <cstdio>
#include <cstring>

#include <vulkan/vulkan.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "array_list.hpp"

#include "vec.hpp"
#include "mat.hpp"

#include "phVkModel.hpp"
#include "phVkMaterial.hpp"


template <typename T>
class phVkScene
{
public:
    ArrayList<std::string> file_paths;

	ArrayList<phVkMesh<T>> meshes;
	ArrayList<phVkMaterial> materials;

	ArrayList<phVkModel<T>> models;

    // -- Functions --
    bool load(std::string path);
};


template <typename T>
void phVkScene<T>::load(std::string path)
{
    unsigned int meshes_offset = meshes.getCount();
    unsigned int materials_offset materials.getCount();

    Assimp::Importer importer;

    // TODO: make configurable
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace |
        aiProcess_GenNormals);


    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        throw std::runtime_error("Failed to load model: " + std::string(importer.GetErrorString()));
    }

    // Save file path
    file_paths.push(path);

    // Load meshes
    // Note: index may not match Assimp index if multiple files have been loaded into the scene
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        processMesh(scene->mMesh[i], scene);
    }

    // Load materials
    // Note: index may not match Assimp index if multiple files have been loaded into the scene
    for (unsigned int i = 0; i < scene->mMaterials; i++)
    {
        processMaterial(scene->mMaterial[i], scene);
    }

    return processNode(scene->mRootNode, scene);
}


template <typename T>
void phVkScene<T>::processNode(aiNode* node, const aiScene* scene)
{
    // Every node becomes a model
    // Can have multiple meshes

    // Check valid node / meshes
    if (node && node->numMeshes > 0)
        int model = models.push(phVkModel<T>());
    else
        return;

    // Check push returned a valid index
    if (model < 0)
        return;

    // Process all meshes in the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        // TODO: load transformation
        // TODO: load name, etc
        // TODO: index needs to be offset if multiple files have been loaded
        models[model].meshes.push(node)
        //aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        //int m = meshes.push(phVkMesh<T>());
        //processMesh(mesh, scene, meshes.get(m));

        // TODO: materials
        // int mt =...

    }

    // Process all child nodes recursively
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}


// TODO: don't return copy of mesh, instead pass a pointer
Mesh processMesh(aiMesh* mesh, const aiScene* scene)
{
    Mesh result;

    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex{};

        // Position
        vertex.pos.x = mesh->mVertices[i].x;
        vertex.pos.y = mesh->mVertices[i].y;
        vertex.pos.z = mesh->mVertices[i].z;

        // Normal
        if (mesh->HasNormals()) {
            vertex.normal.x = mesh->mNormals[i].x;
            vertex.normal.y = mesh->mNormals[i].y;
            vertex.normal.z = mesh->mNormals[i].z;
        }

        // Texture coordinates
        if (mesh->mTextureCoords[0]) {
            vertex.texCoord.x = mesh->mTextureCoords[0][i].x;
            vertex.texCoord.y = mesh->mTextureCoords[0][i].y;
        }
        else {
            vertex.texCoord = glm::vec2(0.0f, 0.0f);
        }

        result.vertices.push_back(vertex);
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            result.indices.push_back(face.mIndices[j]);
        }
    }

    return result;
}
};
