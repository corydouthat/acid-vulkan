// Copyright (c) 2025, Cory Douthat
// Based on vkguide.net - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Mesh loader

#pragma once

#include "phvk_types.h"

#include <unordered_map>
#include <filesystem>

// DEBUG: Replace vertex colors with vertex normals
constexpr bool override_colors = true;

class phVkEngine;

struct GeoSurface 
{
    uint32_t start_index;
    uint32_t count;
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

