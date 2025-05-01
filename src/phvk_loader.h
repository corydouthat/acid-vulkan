// Copyright (c) 2025, Cory Douthat
// Based on vkguide.net - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Mesh loader

#pragma once

#include "phvk_types.h"
#include "phvk_descriptors.h"

#include <unordered_map>
#include <filesystem>

// DEBUG: Replace vertex colors with vertex normals
constexpr bool override_colors = false;

class phVkEngine;

struct GLTFMaterial 
{
    MaterialInstance data;
};

struct GeoSurface 
{
    uint32_t start_index;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset 
{
    std::string name;
   
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers mesh_buffers;
};

// Loader function
// Note: std::optional wraps a type and allows for it to be errored or null to fail safely
std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(phVkEngine* engine, std::filesystem::path file_path);

struct LoadedGLTF : public IRenderable 
{

    // storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> top_nodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptor_pool;

    AllocatedBuffer material_data_buffer;

    phVkEngine* creator;

    ~LoadedGLTF() { clearAll(); };

    virtual void draw(const Mat4f& top_matrix, DrawContext& ctx);

private:

    void clearAll();
};

// Loader function
// Note: std::optional wraps a type and allows for it to be errored or null to fail safely
std::optional<std::shared_ptr<LoadedGLTF>> LoadGLTF(phVkEngine* engine, std::string_view file_path);
