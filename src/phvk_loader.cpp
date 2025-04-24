// Copyright (c) 2025, Cory Douthat
// Based on vkguide.net - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Mesh loader


#include <stb_image.h>
#include <iostream>

#include "phvk_loader.h"

#include "phvk_engine.h"
#include "phvk_initializers.h"
#include "phvk_types.h"

#include "element_traits_for_fastgltf.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGLTFMeshes(phVkEngine* engine, std::filesystem::path file_path)
{
    // TODO: switch to {fmt} (doesn't work for some reason?)
    //fmt::print("Loading glTF: ", file_path.string(), "\n");
	std::cout << "Loading glTF: " << file_path << "\n";

    // Open file
    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);

    constexpr auto gltf_options = fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser {};

    // Process file
    auto load = parser.loadGltfBinary(data.get(), file_path.parent_path(), gltf_options);
    if (load) 
    {
        gltf = std::move(load.get());
    } 
    else 
    {
        fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // Use the same vectors for all meshes so that the memory doesnt reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // Load mesh
    for (fastgltf::Mesh& mesh : gltf.meshes) 
    {
        MeshAsset new_mesh;

        new_mesh.name = mesh.name;

        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface new_surface;
            new_surface.start_index = (uint32_t)indices.size();
            new_surface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vert = vertices.size();

            // Load indexes
            {
                fastgltf::Accessor& index_accessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + index_accessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor,
                    [&](std::uint32_t idx) 
                    {
                        indices.push_back(idx + initial_vert);
                    });
            }

            // Load vertex positions
            {
                fastgltf::Accessor& pos_accessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + pos_accessor.count);

                fastgltf::iterateAccessorWithIndex<Vec3f>(gltf, pos_accessor,
                    [&](Vec3f v, size_t index) 
                    {
                        Vertex new_vert;
                        new_vert.position = Vec3f(v.x, v.y, v.z);
                        new_vert.normal = { 1, 0, 0 };
                        new_vert.color = Vec4f(1.f, 1.f, 1.f, 1.f);
                        new_vert.uv_x = 0;
                        new_vert.uv_y = 0;
                        vertices[initial_vert + index] = new_vert;
                    });
            }

            // Load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<Vec3f>(gltf, gltf.accessors[(*normals).accessorIndex],
                    [&](Vec3f v, size_t index) 
                    {
                        vertices[initial_vert + index].normal = Vec3f(v.x, v.y, v.z);
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<Vec2f>(gltf, gltf.accessors[(*uv).accessorIndex],
                    [&](Vec2f v, size_t index) 
                    {
                        vertices[initial_vert + index].uv_x = v.x;
                        vertices[initial_vert + index].uv_y = v.y;
                    });
            }

            // Load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<Vec4f>(gltf, gltf.accessors[(*colors).accessorIndex],
                    [&](Vec4f v, size_t index) 
                    {
						vertices[initial_vert + index].color = Vec4f(v.x, v.y, v.z, v.w);
                    });
            }
            new_mesh.surfaces.push_back(new_surface);
        }

        // Display the vertex normals
        if (override_colors) {
            for (Vertex& vert : vertices) 
            {
                vert.color = Vec4f(vert.normal, 1.f);
            }
        }
        new_mesh.mesh_buffers = engine->uploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(new_mesh)));
    }

    return meshes;
}
