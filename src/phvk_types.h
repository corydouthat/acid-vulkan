// Copyright (c) 2025, Cory Douthat
// Based on vkguide.net - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Primary includes and type definitions

#pragma once

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <vector>   // TODO: replace with phArrayList?
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <vk_mem_alloc.h>

#include <fmt/core.h>

// TODO: make template or switch to doubles?
#include "vec.hpp"
#include "mat.hpp"


struct AllocatedImage 
{
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;
};

struct AllocatedBuffer 
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct Vertex 
{
    // TODO: will optimize to compact data later in the vkguide.dev tutorial
    // Interleaving uv's to better match shader alignment on GPU
    Vec3f position;
    float uv_x;
    Vec3f normal;
    float uv_y;
    Vec4f color;
};

// Holds the resources needed for a mesh
struct GPUMeshBuffers 
{
    AllocatedBuffer index_buffer;
    AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
};

// Push constants for our mesh object draws
// TODO: re-name to GPUMeshPushConstants?
struct GPUDrawPushConstants 
{
    Mat4f world_matrix;                     // Transform matrix
    VkDeviceAddress vertex_buffer_address;  // Buffer address
};


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
