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
};


// MATERIAL
// TODO


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


