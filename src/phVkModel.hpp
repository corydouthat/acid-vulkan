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

#include "phVkMaterial.hpp"

// VERTEX
template <typename T = float>
struct phVkVertex
{
	Vec3<T> p;		// Position
	Vec3<T> n;		// Normal vector
	Vec2<T> uv;		// Texture coordinate
};


// MESH
template <typename T = float>
struct phVkMesh
{
	std::string name;	// Human friendly name (optional)
	bool ccw = true;	// Counter-clockwise order / right-hand rule

	ArrayList<phVkVertex<T>> v;	// Vertices
	ArrayList<unsigned int> i;	// Vertex indices (triangles)

    void phVkMesh<T>::processMesh(const aiMesh* mesh, const aiScene* scene);
};


// MESH SET
template <typename T = float>
struct phVkMeshSet
{
	unsigned int mesh_i;	// Mesh index
	unsigned int mat_i;		// Material index
	Mat4<T> transform;		// Local mesh transformation
};


// MODEL
template <typename T = float>
struct phVkModel
{
	std::string name;		// Human friendly name (optional)
	std::string file_path;	// Original file path

	ArrayList<phVkMeshSet> sets;	// Mesh sets
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

        v.push(vertex);
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];

        if (face.numIndices == 3)
        {
            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                i.push(face.mIndices[j]);
            }
        }
        else
        {
            // TODO: implement non-triangular faces
            throw std::runtime_error("Failed to load model: " + std::string(importer.GetErrorString()));
        }
    }
}
