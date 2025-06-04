// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Scene class and setup functions

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
	ArrayList<phVkMaterial<T>> materials;

	ArrayList<phVkModel<T>> models;

    // -- Functions --
    void load(std::string path);
    void processNode(aiNode* node, const aiScene* scene, Mat4<T> global_transform = Mat4<T>(), 
        unsigned int meshes_offset = 0, unsigned int materials_offset = 0);
    void processMesh(const aiMesh* mesh, const aiScene* scene, phVkMesh<T>* new_mesh);
    void initVulkan(phVkEngine<T>* engine);

    // No destructor - rely on other objects destructors
};


template <typename T>
void phVkScene<T>::load(std::string path)
{
    // Track index offsets to allow mutliple files to be loaded to a single scene
    unsigned int meshes_offset = meshes.getCount();
    unsigned int materials_offset = materials.getCount();

    Assimp::Importer importer;

    // TODO: make configurable
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |             // Self-explanatory (does allow line and point primitives)
		//aiProcess_ConvertToLeftHanded |   // Convert to left-handed coordinate system (default is right-handed)
        //aiProcess_FlipUVs |               // Flip to clockwise (ccw is default)
		aiProcess_GenUVCoords |             // Generate UV coordinates (if non-standard?)
        aiProcess_TransformUVCoords |       // Normalize UVs if needed
        aiProcess_CalcTangentSpace |        // Used for things like normal mapping
        aiProcess_GenNormals |              // Generate vertex normals (only if they don't exist)
        aiProcess_JoinIdenticalVertices);   // Merge duplicate vertices (but, see note at processMesh)         


    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        throw std::runtime_error("Failed to load model: " + std::string(importer.GetErrorString()));
    }

    // Save file path
    file_paths.push(path);

    // Get model directory for material functions
    std::string model_directory = path.substr(0, path.find_last_of('/'));

    // Load meshes
    // Note: index may not match Assimp index if multiple files have been loaded into the scene
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        int mesh = meshes.push(phVkMesh<T>());  
        //int mesh = meshes.pushEmplace();      // TODO: confirming if this causes memory access violation when deleting?
        meshes[mesh].processMesh(scene->mMeshes[i], scene);
    }

    // Load materials
    // Note: index may not match Assimp index if multiple files have been loaded into the scene
    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        int mat = materials.pushEmplace();
        materials[mat].processMaterial(scene->mMaterials[i], scene, model_directory);
    }

    return processNode(scene->mRootNode, scene, Mat4<T>(), meshes_offset, materials_offset);
}


// Note on global_transform:
// Keeps track of transformation as the node tree is traversed
// Used to expand relative transformations all into global coordinates
template <typename T>
void phVkScene<T>::processNode(aiNode* node, const aiScene* scene, Mat4<T> global_transform, 
    unsigned int meshes_offset, unsigned int materials_offset)
{
    // Every node becomes a model
    // Can have multiple meshes
    // But, there is only a single transform per node

    // Check valid node / meshes
    int model = -1;
    if (node && node->mNumMeshes > 0)
        model = models.push(phVkModel<T>{});
    else
        return;

    // Get node name
    models[model].name = node->mName.C_Str();

    // Get node transformation matrix
    global_transform = global_transform * Mat4<T>(&node->mTransformation[0][0]);
    models[model].transform = global_transform;

    // Process all meshes in the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        // Add mesh set
        int set = models[model].sets.push(phVkMeshSet<T>());

        // Register mesh instance (index)
        models[model].sets[set].mesh_i = meshes_offset + node->mMeshes[i];

        //// Register material instances (indexes)
        //// Note: it appears that Assimp assigns materials at the mesh level, not the mesh instance
        //models[model].sets[set].mat_i = materials_offset + scene->mMeshes[node->mMeshes[i]]->mMaterialIndex;
    }

    // Process all child nodes recursively
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene, global_transform, meshes_offset, materials_offset);
    }
}


template <typename T>
void phVkScene<T>::initVulkan(phVkEngine<T>* engine)
{
    for (int i = 0; i < meshes.getCount(); i++)
    {
        meshes[i].initVulkan(engine);
    }

    for (int i = 0; i < materials.getCount(); i++)
    {
        materials[i].initVulkan(engine);
    }
}
