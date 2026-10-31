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

    
    // -- Constructors and Operators --
	phVkScene<T>() = default;  // Default constructor
    // phVkMaterial is non-copyable so disable copy/assign and enable move
    phVkScene<T>(const phVkScene<T>& copy) = delete;
    phVkScene<T>(phVkScene<T>&& move) noexcept = default;
    const phVkScene<T>& operator =(const phVkScene<T>& copy) = delete;
    phVkScene<T>& operator =(phVkScene<T>&& move) noexcept = default;

    // -- Functions --
    void load(std::string path);
    void processNode(aiNode* node, const aiScene* scene, Mat4<T> global_transform = Mat4<T>(),
        unsigned int meshes_offset = 0, unsigned int materials_offset = 0, int parent_model = -1);
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
        //aiProcess_Triangulate |               // Self-explanatory (does allow line and point primitives)
		//aiProcess_ConvertToLeftHanded |       // Convert to left-handed coordinate system (default is right-handed)
        //aiProcess_FlipUVs |                   // Flip to clockwise (ccw is default)
		aiProcess_GenUVCoords |                 // Generate UV coordinates (if non-standard?)
        aiProcess_TransformUVCoords |           // Normalize UVs if needed
        //aiProcess_CalcTangentSpace |          // Used for things like normal mapping
        //aiProcess_GenNormals |                // Generate vertex normals (does it recalculate if they exist?)
        //aiProcess_JoinIdenticalVertices |     // Merge duplicate vertices (but, see note at processMesh)
        0
    );


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
    unsigned int meshes_offset, unsigned int materials_offset, int parent_model)
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

    // Nodes with meshes become models, and so do empties that carry custom properties
    // (spawn points, pivots, etc.) - a model with no meshes just doesn't draw
    // Note: Blender only writes extras with "Custom Properties" checked on export,
    //       and only object properties end up on the node
    bool has_props = node->mMetaData && node->mMetaData->mNumProperties > 0;

    int model = -1;
    if (node->mNumMeshes > 0 || has_props)
    {
        model = models.push(phVkModel<T>{});

        // Get node name
        models[model].name = node->mName.C_Str();
        models[model].parent = parent_model;

        // Copy custom properties (see phVkNodeProp)
        for (unsigned int i = 0; has_props && i < node->mMetaData->mNumProperties; i++)
        {
            const char* key = node->mMetaData->mKeys[i].C_Str();
            const aiMetadataEntry& value = node->mMetaData->mValues[i];

            phVkNodeProp<T> prop = {};

            switch (value.mType)
            {
            case AI_BOOL:
                prop.type = phVkNodeProp<T>::PROP_BOOL;
                prop.flag = *static_cast<const bool*>(value.mData);
                break;
            case AI_INT32:
                prop.type = phVkNodeProp<T>::PROP_NUMBER;
                prop.number = *static_cast<const int32_t*>(value.mData);
                break;
            case AI_UINT64:
                prop.type = phVkNodeProp<T>::PROP_NUMBER;
                prop.number = (double)*static_cast<const uint64_t*>(value.mData);
                break;
            case AI_FLOAT:
                prop.type = phVkNodeProp<T>::PROP_NUMBER;
                prop.number = *static_cast<const float*>(value.mData);
                break;
            case AI_DOUBLE:
                prop.type = phVkNodeProp<T>::PROP_NUMBER;
                prop.number = *static_cast<const double*>(value.mData);
                break;
            case AI_AISTRING:
                prop.type = phVkNodeProp<T>::PROP_STRING;
                strncpy(prop.text, static_cast<const aiString*>(value.mData)->C_Str(), sizeof(prop.text) - 1);
                if (strlen(static_cast<const aiString*>(value.mData)->C_Str()) >= sizeof(prop.text))
                    printf("phVkScene: property '%s' on '%s' truncated to %zu chars\n", key, node->mName.C_Str(), sizeof(prop.text) - 1);
                break;
            case AI_AIMETADATA:
            {
                // Arrays come through as nested metadata with one numeric child per element
                // (nested objects also land here - skipped below if anything isn't a number)
                const aiMetadata* sub = static_cast<const aiMetadata*>(value.mData);
                bool numeric = sub && sub->mNumProperties > 0 && sub->mNumProperties <= 16;

                for (unsigned int k = 0; numeric && k < sub->mNumProperties; k++)
                {
                    // Array elements all carry the array's own name - anything else is a nested object
                    if (strcmp(sub->mKeys[k].C_Str(), key) != 0)
                    {
                        numeric = false;
                        break;
                    }

                    const aiMetadataEntry& e = sub->mValues[k];
                    switch (e.mType)
                    {
                    case AI_INT32:  prop.values[k] = *static_cast<const int32_t*>(e.mData); break;
                    case AI_UINT64: prop.values[k] = (double)*static_cast<const uint64_t*>(e.mData); break;
                    case AI_FLOAT:  prop.values[k] = *static_cast<const float*>(e.mData); break;
                    case AI_DOUBLE: prop.values[k] = *static_cast<const double*>(e.mData); break;
                    default:        numeric = false; break;
                    }
                }

                if (!numeric)
                {
                    printf("phVkScene: property '%s' on '%s' skipped (only numeric arrays up to 16 are supported)\n", key, node->mName.C_Str());
                    continue;
                }

                prop.type = phVkNodeProp<T>::PROP_ARRAY;
                prop.count = (unsigned char)sub->mNumProperties;
                break;
            }
            default:    // AI_AIVECTOR3D and anything newer
                printf("phVkScene: property '%s' on '%s' skipped (unsupported type %d)\n", key, node->mName.C_Str(), (int)value.mType);
                continue;
            }

            strncpy(prop.key, key, sizeof(prop.key) - 1);
            if (strlen(key) >= sizeof(prop.key))
                printf("phVkScene: property key '%s' on '%s' truncated to %zu chars\n", key, node->mName.C_Str(), sizeof(prop.key) - 1);

            models[model].props.push(prop);
        }

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
    // Process all child nodes recursively
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        // Children of a skipped node inherit this node's parent
        processNode(node->mChildren[i], scene, global_transform, meshes_offset, materials_offset,
            model >= 0 ? model : parent_model);
    }
}


template <typename T>
void phVkScene<T>::initVulkan(phVkEngine<T>* engine)
{
    for (unsigned int i = 0; i < meshes.getCount(); i++)
    {
        meshes[i].initVulkan(engine);
    }

    for (unsigned int i = 0; i < materials.getCount(); i++)
    {
        materials[i].initVulkan(engine);
    }
}
