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

enum class MaterialPass : uint8_t 
{
    main_color,
    transparent,
    other
};

struct MaterialPipeline 
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance 
{
    MaterialPipeline* pipeline;
    VkDescriptorSet material_set;
    MaterialPass pass_type;
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

struct GPUSceneData 
{
    Mat4f view;
    Mat4f proj;
    Mat4f view_proj;
    Vec4f ambient_color;
    Vec4f sunlight_direction; // w for sun power
    Vec4f sunlight_color;
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

// Base class for a renderable dynamic object
struct DrawContext;     // Forward declaration
class IRenderable 
{
    virtual void draw(const Mat4f& top_matrix, DrawContext& ctx) = 0;
};

// Implementation of a drawable scene node. The scene node 
// can hold children and will also keep a transform to propagate to them
struct Node : public IRenderable 
{

    // Parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    Mat4f local_transform;
    Mat4f world_transform;

    void refreshTransform(const Mat4f& parent_matrix)
    {
        world_transform = parent_matrix * local_transform;
        for (auto c : children) 
        {
            c->refreshTransform(world_transform);
        }
    }

    virtual void Draw(const Mat4f& top_matrix, DrawContext& ctx)
    {
        // Draw children
        for (auto& c : children) 
        {
            c->Draw(top_matrix, ctx);
        }
    }
};

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
