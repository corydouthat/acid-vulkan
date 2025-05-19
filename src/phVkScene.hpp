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
    void processNode(aiNode* node, const aiScene* scene, Mat4<T> global_transform = Mat4<T>(), 
        unsigned int meshes_offset = 0, unsigned int material_offset = 0);
    void processMesh(const aiMesh* mesh, const aiScene* scene, phVkMesh<T>* new_mesh)
};


template <typename T>
void phVkScene<T>::load(std::string path)
{
    // Track index offsets to allow mutliple files to be loaded to a single scene
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
        int mesh = meshes.push(phVkMesh<T>{});
        processMesh(scene->mMesh[i], scene, &meshes[mesh]);
    }

    // Load materials
    // Note: index may not match Assimp index if multiple files have been loaded into the scene
    for (unsigned int i = 0; i < scene->mMaterials; i++)
    {
        int mat = materials.push(phVkMaterial<T>());
        processMaterial(scene->mMaterial[i], scene, &materials[mat]);
    }

    return processNode(scene->mRootNode, scene, Mat4<T>(), meshes_offset, material_offset);
}


// Note on global_transform:
// Keeps track of transformation as the node tree is traversed
// Used to expand relative transformations all into global coordinates
template <typename T>
void phVkScene<T>::processNode(aiNode* node, const aiScene* scene, Mat4<T> global_transform, 
    unsigned int meshes_offset, unsigned int material_offset)
{
    // Every node becomes a model
    // Can have multiple meshes
    // But, there is only a single transform per node

    // Check valid node / meshes
    if (node && node->numMeshes > 0)
        int model = models.push(phVkModel<T>{});
    else
        return;

    // Get node name
    models[model].name = node->mName;

    // Get node transformation matrix
    global_transform = global_transform * Mat4<T>(node->mTransformation);
    models[model].transform = global_transform;

    // Process all meshes in the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        // Add mesh set
        int set = models[model].sets.push(phVkMeshSet<T>());

        // Register mesh instance (index)
        models[model].sets[set].mesh_i = meshes_offset + node->mMesh[i];

        // Register material instances (indexes)
        // Note: it appears that Assimp assigns materials at the mesh level, not the mesh instance
        models[model].sets[set].mat_i = materials_offset + scene->mMeshes[node->mMesh[i]]->mMaterialIndex;
    }

    // Process all child nodes recursively
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene, global_transform, meshes_offset, material_offset);
    }
}


template <typename T>
void phVkScene<T>::processMesh(const aiMesh* mesh, const aiScene* scene, phVkMesh<T>* new_mesh)
{
    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) 
    {
        phVkVertex<T> vertex{};

        // Position
        vertex.p.x = mesh->mVertices[i].x;
        vertex.p.y = mesh->mVertices[i].y;
        vertex.p.z = mesh->mVertices[i].z;

        // Normal
        if (mesh->HasNormals()) 
        {
            vertex.n.x = mesh->mNormals[i].x;
            vertex.n.y = mesh->mNormals[i].y;
            vertex.n.z = mesh->mNormals[i].z;
        }

        // Texture coordinates
        if (mesh->hasTextureCoords())
        {
            // TODO: add support for AI_MAX_NUMBER_OF_TEXTURECOORDS != 2
            vertex.uv.x = mesh->mTextureCoords[0];
            vertex.uv.y = mesh->mTextureCoords[1];
        }
        else 
        {
            vertex.uv = glm::vec2(0.0f, 0.0f);
        }

        new_mesh.v.push(vertex);
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) 
    {
        aiFace face = mesh->mFaces[i];

        if (face.numIndices == 3)
        {
            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                new_mesh->i.push(face.mIndices[j]);
            }
        }
        else
        {
            // TODO: implement non-triangular faces
            throw std::runtime_error("Failed to load model: " + std::string(importer.GetErrorString()));
        }
    }
}

