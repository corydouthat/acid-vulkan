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
    // Interleave uv's to match shader alignment, and supposedly better match GPU memory?
    Vec3<T> p;
    float uv_x;
    Vec3<T> n;
    float uv_y;
    Vec4<T> c;

	//Vec3<T> p;		// Position
	//Vec3<T> n;		// Normal vector
	//Vec2<T> uv;		// Texture coordinate
	//Vec4<T> c;	    // Color (optional, for vertex colors)
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
	int mat_i;                          // Material index (-1 if none)

    // Vulkan buffer data
    phVkEngine<T>* engine;
    AllocatedBuffer index_buffer;
    AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;

    // Constructor + Destructor
    phVkMesh();
    ~phVkMesh() { vulkanCleanup(); }

    // Functions
    void processMesh(const aiMesh* mesh, const aiScene* scene, unsigned int materials_offset = 0);
    void initVulkan(phVkEngine<T>* engine);
    void vulkanCleanup();
};


// MODEL
template <typename T = float>
struct phVkModel
{
	std::string name;		// Human friendly name (optional)
	std::string file_path;	// Original file path

	ArrayList<unsigned int> mesh_indices; // Mesh indices (can have multiple meshes per model)

    bool scale_valid = false;
	Mat4<T> scale;              // Scale matrix

    bool transform_valid = false;
    Mat4<T> transform;          // Object transform to global coordinates

	bool scale_transform_valid = false;
    Mat4<T> scale_transform;    // Combined transform matrix

    // Callback function and index to get transform from external engine
    unsigned int ext_index = 0;
	std::function<Mat4<T>(unsigned int)> getExtTransform = nullptr;
	//Mat4<T>(*getExtTransform)(unsigned int) = nullptr;

    void setTransformCallback(std::function<Mat4<T>(unsigned int)>, unsigned int index);
    Mat4<T> getTransform();
};


template <typename T>
phVkMesh<T>::phVkMesh()
{
    ccw = true;
    mat_i = -1;
    engine = nullptr;
    index_buffer = {};
    vertex_buffer = {};
    vertex_buffer_address = 0;
}


// Note: assimp will generate duplicate vertices in order to support
//       multiple texture coordinates, normals, etc.
//       As an example, a cube resulted in 24 vertices.
template <typename T>
void phVkMesh<T>::processMesh(const aiMesh* mesh, const aiScene* scene, unsigned int materials_offset)
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
        if (mesh->HasTextureCoords(0))
        {
            // TODO: add support for AI_MAX_NUMBER_OF_TEXTURECOORDS != 2
            // TODO: add support for additional texture coordinate sets
            vertex.uv_x = mesh->mTextureCoords[0][i].x;
            vertex.uv_y = 1.0f - mesh->mTextureCoords[0][i].y;
        }
        else
        {
            vertex.uv_x = vertex.uv_y = 0.0f;// Vec2<T>(0.0f, 0.0f);
        }

        if (mesh->HasVertexColors(0))
        {
			vertex.c = Vec4<T>(
                mesh->mColors[0][i].r,
                mesh->mColors[0][i].g,
                mesh->mColors[0][i].b,
                mesh->mColors[0][i].a); // First color channel
		}
        else
        {
            // TODO: add configurable default value
            vertex.c = Vec4<T>(191.0 / 255.0f, 64.0 / 255.0f, 191.0 / 255.0f, 1.0f);
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

    // Process materials
    mat_i = mesh->mMaterialIndex + materials_offset;
}

template <typename T>
void phVkMesh<T>::initVulkan(phVkEngine<T>* engine)
{
    if (!engine)
        return;

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

	phVkVertex<T>* temp = (phVkVertex<T>*)data;

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
    if (!engine)
        return;
    
    if (index_buffer.buffer)
        engine->destroyBuffer(index_buffer);
    if (vertex_buffer.buffer)
        engine->destroyBuffer(vertex_buffer);
}


template <typename T>
void phVkModel<T>::setTransformCallback(std::function<Mat4<T>(unsigned int)> func, unsigned int index)
{
    getExtTransform = func;
    ext_index = index;
}


template <typename T>
Mat4<T> phVkModel<T>::getTransform()
{
    Mat4<T> temp;

    if (getExtTransform)
    {
        temp = getExtTransform(ext_index);

        if (transform_valid)
            transform = temp;

        if (scale_valid)
            temp = temp * scale;

        if (scale_transform_valid)
            scale_transform = temp;

        return temp;
    }
    else
    {
        if (scale_transform_valid)
        {
            return scale_transform;
        }
        else
        {
            if (scale_valid)
                temp = scale;

            if (transform_valid)
                temp = transform * temp;

            return temp;
        }
    }
}
