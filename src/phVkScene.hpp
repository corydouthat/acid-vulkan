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
    //void processMesh(const aiMesh* mesh, const aiScene* scene, phVkMesh<T>* new_mesh, 
    //    unsigned int materials_offset = 0);
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

    // Load materials
    // Note: index may not match Assimp index if multiple files have been loaded into the scene
    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        int mat = materials.pushEmplace();
        materials[mat].processMaterial(scene->mMaterials[i], scene, model_directory);
    }

    // Load meshes
    // Note: index may not match Assimp index if multiple files have been loaded into the scene
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        int mesh = meshes.pushEmplace();
        meshes[mesh].processMesh(scene->mMeshes[i], scene, materials_offset);
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
    if (!node || !scene)
        return;

    // Every node becomes a model
    // Can have multiple meshes
    // But, there is only a single transform per node

    Mat4<T> ai_transform, model_transform, model_scale;

    // Note: aiMatrix4x4 is in row-major order, so weneed to transpose it
    ai_transform = Mat4<T>(&node->mTransformation[0][0]).transp();

	// Decompose transform to separate out scaling
    ai_transform.decomposeTransfScale(&model_transform, &model_scale);

	// Propogate node transforms and update global_transform
    ai_transform = global_transform * ai_transform;
    global_transform = global_transform * model_transform;

    int model = -1;
    if (node->mNumMeshes > 0)   // Check valid node / meshes
    {
        model = models.push(phVkModel<T>{});

        // Get node name
        models[model].name = node->mName.C_Str();

        // Set node scale matrix
        models[model].scale = model_scale;
        if (model_scale != Mat4<T>())
			models[model].scale_valid = true;

		// Set node transform matrix
		models[model].transform = global_transform;
        models[model].transform_valid = true;

		// Set combined scale transform matrix
		models[model].scale_transform = ai_transform;
		models[model].scale_transform_valid = true;

        // Process all meshes in the current node
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // Register mesh instance (index)
            int mesh_instance = models[model].mesh_indices.push(meshes_offset + node->mMeshes[i]);
        }
    }
    //else
    //{
    //    // TODO: define empty node if relevant?
    //}

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
