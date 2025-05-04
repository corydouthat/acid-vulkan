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
#include <fastgltf/util.hpp>

#include <variant>

std::optional<AllocatedImage> LoadImage(phVkEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    AllocatedImage new_image{};

    int width, height, num_channels;

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0);   // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath());     // We're only capable of loading
                                                        // local files.

                const std::string path(filePath.uri.path().begin(),
                    filePath.uri.path().end()); // Thanks C++.
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &num_channels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    new_image = engine->createImage(data, imagesize, 
                        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                    stbi_image_free(data);
                }
                },
                [&](fastgltf::sources::Vector& vector) {
                    unsigned char* data = stbi_load_from_memory(
                        reinterpret_cast<const stbi_uc *>(vector.bytes.data()), 
                        static_cast<int>(vector.bytes.size()),
                        &width, &height, &num_channels, 4);
                    if (data) {
                        VkExtent3D imagesize;
                        imagesize.width = width;
                        imagesize.height = height;
                        imagesize.depth = 1;

                        new_image = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                        stbi_image_free(data);
                    }
                },
                [&](fastgltf::sources::BufferView& view) {
                    auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                    auto& buffer = asset.buffers[bufferView.bufferIndex];

                    std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                        // specify LoadExternalBuffers, meaning all buffers
                        // are already loaded into a vector.
                [](auto& arg) {},
                [&](fastgltf::sources::Vector& vector) {
                    unsigned char* data = stbi_load_from_memory(
                        reinterpret_cast<const stbi_uc*>(vector.bytes.data()) + bufferView.byteOffset,
                        static_cast<int>(bufferView.byteLength),
                        &width, &height, &num_channels, 4);
                    if (data) {
                        VkExtent3D imagesize;
                        imagesize.width = width;
                        imagesize.height = height;
                        imagesize.depth = 1;

                        new_image = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                            VK_IMAGE_USAGE_SAMPLED_BIT,false);

                        stbi_image_free(data);
                    }
                } },
                buffer.data);
                },
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (new_image.image == VK_NULL_HANDLE) 
    {
        return {};
    }
    else 
    {
        return new_image;
    }
}

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

//> filters
VkFilter ExtractFilter(fastgltf::Filter filter)
{
    switch (filter) 
    {
    // Nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    // Linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter) 
    {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}
//< filters

std::optional<std::shared_ptr<LoadedGLTF>> LoadGLTF(phVkEngine* engine, std::string_view file_path)
{
    fmt::print("Loading GLTF: {}", file_path);
	fmt::print("\n");

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser{};

    constexpr auto gltf_options = 
        fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | 
        fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    //fastgltf::GltfDataBuffer data;
    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);

    fastgltf::Asset gltf;

    std::filesystem::path path = file_path;

    auto type = fastgltf::determineGltfFileType(data.get());
    if (type == fastgltf::GltfType::glTF) 
    {
        auto load = parser.loadGltf(data.get(), path.parent_path(), gltf_options);

        if (load) 
        {
            gltf = std::move(load.get());
        }
        else 
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB) 
    {
        auto load = parser.loadGltfBinary(data.get(), path.parent_path(), gltf_options);

        if (load) 
        {
            gltf = std::move(load.get());
        }
        else 
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else 
    {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }

    // We can stimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { 
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };

    file.descriptor_pool.init(engine->device, gltf.materials.size(), sizes);

    // Load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers)
    {

        VkSamplerCreateInfo sampl = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler new_sampler;
        vkCreateSampler(engine->device, &sampl, nullptr, &new_sampler);

        file.samplers.push_back(new_sampler);
    }

    // Temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // load all textures
    for (fastgltf::Image& image : gltf.images) 
    {
        std::optional<AllocatedImage> img = LoadImage(engine, gltf, image);

        if (img.has_value()) 
        {
            images.push_back(*img);
            file.images[image.name.c_str()] = *img;
        }
        else 
        {
            // we failed to load, so lets give the slot a default white texture to not
            // completely break loading
            images.push_back(engine->error_checkerboard_image);
            std::cout << "gltf failed to load texture " << image.name << std::endl;
        }
    }

    // Load all textures
    for (fastgltf::Image& image : gltf.images) 
    {
        // TEMP
        images.push_back(engine->error_checkerboard_image);
    }

    // Create buffer to hold the material data
    file.material_data_buffer = engine->createBuffer(sizeof(GLTFMetallicRoughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;
    GLTFMetallicRoughness::MaterialConstants* scene_material_constants = (GLTFMetallicRoughness::MaterialConstants*)file.material_data_buffer.info.pMappedData;

    for (fastgltf::Material& mat : gltf.materials) 
    {
        std::shared_ptr<GLTFMaterial> new_mat = std::make_shared<GLTFMaterial>();
        materials.push_back(new_mat);
        file.materials[mat.name.c_str()] = new_mat;

        GLTFMetallicRoughness::MaterialConstants constants;
        constants.color_factors.x = mat.pbrData.baseColorFactor[0];
        constants.color_factors.y = mat.pbrData.baseColorFactor[1];
        constants.color_factors.z = mat.pbrData.baseColorFactor[2];
        constants.color_factors.w = mat.pbrData.baseColorFactor[3];

        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
        
        // Write material parameters to buffer
        scene_material_constants[data_index] = constants;

        MaterialPass passType = MaterialPass::main_color;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::transparent;
        }

        GLTFMetallicRoughness::MaterialResources material_resources;
        // default the material textures
        material_resources.color_image = engine->white_image;
        material_resources.color_sampler = engine->default_sampler_linear;
        material_resources.metal_rough_image = engine->white_image;
        material_resources.metal_rough_sampler = engine->default_sampler_linear;

        // set the uniform buffer for the material data
        material_resources.data_buffer = file.material_data_buffer.buffer;
        material_resources.data_buffer_offset = data_index * sizeof(GLTFMetallicRoughness::MaterialConstants);
        
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) 
        {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            material_resources.color_image = images[img];
            material_resources.color_sampler = file.samplers[sampler];
        }
        
        // build material
        new_mat->data = engine->metal_rough_material.writeMaterial(engine->device, passType, material_resources, file.descriptor_pool);

        data_index++;
    }

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) 
    {
        std::shared_ptr<MeshAsset> new_mesh = std::make_shared<MeshAsset>();
        meshes.push_back(new_mesh);
        file.meshes[mesh.name.c_str()] = new_mesh;
        new_mesh->name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) 
        {
            GeoSurface newSurface;
            newSurface.start_index = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) 
                    {
                        indices.push_back(idx + initial_vtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<Vec3f>(gltf, posAccessor,
                    [&](Vec3f v, size_t index) 
                    {
                        Vertex new_vtx;
                        new_vtx.position = v;
                        new_vtx.normal = { 1, 0, 0 };
                        new_vtx.color = Vec4f(1.f, 1.f, 1.f, 1.f);
                        new_vtx.uv_x = 0;
                        new_vtx.uv_y = 0;
                        vertices[initial_vtx + index] = new_vtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) 
            {
                fastgltf::iterateAccessorWithIndex<Vec3f>(gltf, gltf.accessors[(*normals).accessorIndex],
                    [&](Vec3f v, size_t index) 
                    {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<Vec2f>(gltf, gltf.accessors[(*uv).accessorIndex],
                    [&](Vec2f v, size_t index) 
                    {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) 
            {
                fastgltf::iterateAccessorWithIndex<Vec4f>(gltf, gltf.accessors[(*colors).accessorIndex],
                    [&](Vec4f v, size_t index) 
                    {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            if (p.materialIndex.has_value()) 
            {
                newSurface.material = materials[p.materialIndex.value()];
            }
            else 
            {
                newSurface.material = materials[0];
            }

            new_mesh->surfaces.push_back(newSurface);
        }

        new_mesh->mesh_buffers = engine->uploadMesh(indices, vertices);
    }

    // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes) 
    {
        std::shared_ptr<Node> new_node;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) 
        {
            new_node = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(new_node.get())->mesh = meshes[*node.meshIndex];
        }
        else 
        {
            new_node = std::make_shared<Node>();
        }

        nodes.push_back(new_node);
        file.nodes[node.name.c_str()];

        std::visit(fastgltf::visitor{ [&](fastgltf::math::fmat4x4 matrix) {
                                  memcpy(&new_node->local_transform, matrix.data(), sizeof(matrix));
                              },
               [&](fastgltf::TRS transform) {
                   Vec3f tl(transform.translation[0], transform.translation[1],
                       transform.translation[2]);
                   Quatf rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                       transform.rotation[2]);
                   Vec3f sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                   Mat4f tm = Mat4f::transl(tl);
                   Mat4f rm = Mat4f(Mat3f::rot(rot));
                   Mat4f sm = Mat4f(Mat3f::scale(sc));

                   new_node->local_transform = tm * rm * sm;
               } },
            node.transform);
    }

    // run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) 
    {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& scene_node = nodes[i];

        for (auto& c : node.children) {
            scene_node->children.push_back(nodes[c]);
            nodes[c]->parent = scene_node;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : nodes) 
    {
        if (node->parent.lock() == nullptr) {
            file.top_nodes.push_back(node);
            node->refreshTransform(Mat4f());
        }
    }
    return scene;
}

void LoadedGLTF::draw(const Mat4f& top_matrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : top_nodes) 
    {
        n->draw(top_matrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->device;

    for (auto& [k, v] : meshes) {

        creator->destroyBuffer(v->mesh_buffers.index_buffer);
        creator->destroyBuffer(v->mesh_buffers.vertex_buffer);
    }

    for (auto& [k, v] : images) {

        if (v.image == creator->error_checkerboard_image.image) 
        {
            // dont destroy the default images
            continue;
        }
        creator->destroyImage(v);
    }

    for (auto& sampler : samplers) 
    {
        vkDestroySampler(dv, sampler, nullptr);
    }

    auto materialBuffer = material_data_buffer;
    auto samplersToDestroy = samplers;

    descriptor_pool.destroyPools(dv);

    creator->destroyBuffer(materialBuffer);
}
