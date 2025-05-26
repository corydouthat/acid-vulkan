// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Model and Mesh structures

#pragma once

#include <iostream>
#include <cstdio>
#include <cstring>

#include <vulkan/vulkan.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>

#include "array_list.hpp"

#include "vec.hpp"
#include "mat.hpp"

#include "phVkEngine.hpp"
#include "phVkTypes.hpp"
#include "phVkMaterial.hpp"


template <typename T>
class phVkEngine;

// VERTEX
template <typename T = float>
struct phVkVertex
{
    // TODO: InterleavE uv's to better match shader alignment on GPU, like this:
    //Vec3f position;
    //float uv_x;
    //Vec3f normal;
    //float uv_y;
    //Vec4f color;

	Vec3<T> p;		// Position
	Vec3<T> n;		// Normal vector
	Vec2<T> uv;		// Texture coordinate
};


// MESH
template <typename T = float>
class phVkMesh
{
public:
	std::string name;	// Human friendly name (optional)
	bool ccw = true;	// Counter-clockwise order / right-hand rule

	ArrayList<phVkVertex<T>> vertices;	// Vertices
	ArrayList<unsigned int> indices;	// Vertex indices (triangles)

    // Vulkan buffer data
    phVkEngine<T>* engine;
    AllocatedBuffer index_buffer;
    AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;

    // Functions
    void processMesh(const aiMesh* mesh, const aiScene* scene);
    void initVulkan(phVkEngine<T>* engine);
    void vulkanCleanup();

    ~phVkMesh() { vulkanCleanup(); }
};


// MESH SET
template <typename T = float>
struct phVkMeshSet
{
	unsigned int mesh_i;	// Mesh index
	unsigned int mat_i;		// Material index
};


// MODEL
template <typename T = float>
struct phVkModel
{
	std::string name;		// Human friendly name (optional)
	std::string file_path;	// Original file path

    Mat4<T> transform;      // Object transform to global coordinates

	ArrayList<phVkMeshSet<T>> sets;	// Mesh sets
};


template <typename T>
void phVkMesh<T>::processMesh(const aiMesh* mesh, const aiScene* scene)
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
        if (mesh->HasTextureCoords(i))
        {
            // TODO: add support for AI_MAX_NUMBER_OF_TEXTURECOORDS != 2
            vertex.uv.x = mesh->mTextureCoords[i]->x;
            vertex.uv.y = mesh->mTextureCoords[i]->y;
        }
        else
        {
            vertex.uv = Vec2<T>(0.0f, 0.0f);
        }

        vertices.push(vertex);
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];

        if (face.mNumIndices == 3)
        {
            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                indices.push(face.mIndices[j]);
            }
        }
        else
        {
            // TODO: implement non-triangular faces
            throw std::runtime_error("Assimp importer found non-triangular faces.");
        }
    }
}

template <typename T>
void phVkMesh<T>::initVulkan(phVkEngine<T>* engine)
{
    unsigned int vertex_buf_size = vertices.getCount() * sizeof(phVkVertex<T>);
    unsigned int index_buf_size = indices.getCount() * sizeof(uint32_t);

    this->engine = engine;

    // Create vertex buffer
    // SSBO | memory copy
    vertex_buffer = engine->createBuffer(vertex_buf_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Find the adress of the vertex buffer
    VkBufferDeviceAddressInfo device_addr_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = vertex_buffer.buffer };
    vertex_buffer_address = vkGetBufferDeviceAddress(engine->device, &device_addr_info);

    // Create index buffer
    // Index draws | memory copy
    index_buffer = engine->createBuffer(index_buf_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);


    // -- Copy Data to GPU --

    // Set up a staging buffer
    AllocatedBuffer staging = engine->createBuffer(vertex_buf_size + index_buf_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // Copy vertex buffer to staging
    memcpy(data, vertices.getData(), vertex_buf_size);
    // Copy index buffer to staging
    memcpy((char*)data + vertex_buf_size, indices.getData(), index_buf_size);

    // Submit commands to copy from staging to GPU buffer
    engine->immediateSubmit([&](VkCommandBuffer cmd)
        {
            VkBufferCopy vertex_copy{ 0 };
            vertex_copy.dstOffset = 0;
            vertex_copy.srcOffset = 0;
            vertex_copy.size = vertex_buf_size;

            vkCmdCopyBuffer(cmd, staging.buffer, vertex_buffer.buffer, 1, &vertex_copy);

            VkBufferCopy index_copy{ 0 };
            index_copy.dstOffset = 0;
            index_copy.srcOffset = vertex_buf_size;
            index_copy.size = index_buf_size;

            vkCmdCopyBuffer(cmd, staging.buffer, index_buffer.buffer, 1, &index_copy);
        });

    // Destroy temporary staging buffer
    engine->destroyBuffer(staging);
}


template <typename T>
void phVkMesh<T>::vulkanCleanup()
{
    engine->destroyBuffer(index_buffer);
    engine->destroyBuffer(vertex_buffer);
}
