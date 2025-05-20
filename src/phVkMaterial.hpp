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

// Do not include in header file
// Only add once?
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "array_list.hpp"

#include "vec.hpp"
#include "mat.hpp"


// Texture data structure
struct phVkTexture 
{
    unsigned char* data = nullptr;

    std::string path;

    bool isLoaded = false;

    int width = 0;
    int height = 0;
    int channels = 0;

    // Constructor
    phVkTexture() = default;

    // Destructor
    ~phVkTexture() 
    {
        if (data) 
        {
            stbi_image_free(data);
            data = nullptr;
        }
    }

    // Disable copy operations
    // TODO: is this necessary?
    phVkTexture(const phVkTexture&) = delete;
    phVkTexture& operator=(const phVkTexture&) = delete;

    // Move constructor
    phVkTexture(phVkTexture&& other) noexcept
        : data(other.data), width(other.width), height(other.height),
        channels(other.channels), path(std::move(other.path)), isLoaded(other.isLoaded) 
    {
        other.data = nullptr;
        other.isLoaded = false;
    }

    // Move assignment operator
    phVkTexture& operator=(phVkTexture&& other) noexcept 
    {
        if (this != &other) 
        {
            if (data) 
            {
                stbi_image_free(data);
            }

            data = other.data;
            width = other.width;
            height = other.height;
            channels = other.channels;
            path = std::move(other.path);
            isLoaded = other.isLoaded;

            other.data = nullptr;
            other.isLoaded = false;
        }
        return *this;
    }
};


class phVkMaterial
{
public:
    std::string name;		// Human friendly name (optional)
    std::string file_path;	// Original file path

    // Basic colors
    float diffuse_color[4];
    float specular_color[4];
    float ambient_color[4];
    float emissive_color[4];

    // Other properties
    float shininess;
    float opacity;
    float reflectivity;
    float refractive_index; // Index of refraction

    // Texture objects
    std::unique_ptr<phVkTexture> diffuse_texture;
    std::unique_ptr<phVkTexture> specular_texture;
    std::unique_ptr<phVkTexture> normal_texture;
    std::unique_ptr<phVkTexture> height_texture;

    // Default constructor with some reasonable values
    phVkMaterial() 
    {
        // Initialize with default values
        diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 0.8f; diffuse_color[3] = 1.0f;
        specular_color[0] = specular_color[1] = specular_color[2] = 0.0f; specular_color[3] = 1.0f;
        ambient_color[0] = ambient_color[1] = ambient_color[2] = 0.2f; ambient_color[3] = 1.0f;
        emissive_color[0] = emissive_color[1] = emissive_color[2] = 0.0f; emissive_color[3] = 1.0f;

        shininess = 0.0f;
        opacity = 1.0f;
        reflectivity = 0.0f;
        refractive_index = 1.0f;

        diffuse_texture = std::make_unique<phVkTexture>();
        specular_texture = std::make_unique<phVkTexture>();
        normal_texture = std::make_unique<phVkTexture>();
        height_texture = std::make_unique<phVkTexture>();
    }
};


// TODO
// Main function to process material data from a loaded model
void phVkMaterial::processMaterial(const aiMaterial* mat, const aiScene* scene)
{
    // Get material name
    aiString name;
    if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) 
    {
        this->name = name;
        std::cout << "Material " << i << ": " << name.C_Str() << std::endl;
    }

    // Get material colors
    aiColor4D color;

    // Diffuse color
    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) 
    {
        diffuse_color[0] = color.r;
        diffuse_color[1] = color.g;
        diffuse_color[2] = color.b;
        diffuse_color[3] = color.a;
    }

    // Specular color
    if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) 
    {
        specular_color[0] = color.r;
        specular_color[1] = color.g;
        specular_color[2] = color.b;
        specular_color[3] = color.a;
    }

    // Ambient color
    if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) 
    {
        ambient_color[0] = color.r;
        ambient_color[1] = color.g;
        ambient_color[2] = color.b;
        ambient_color[3] = color.a;
    }

    // Emissive color
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) 
    {
        emissive_color[0] = color.r;
        emissive_color[1] = color.g;
        emissive_color[2] = color.b;
        emissive_color[3] = color.a;
    }

    // Get material properties
    float value;

    // Shininess
    if (aiMat->Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS) 
    {
        shininess = value;
    }

    // Opacity
    if (aiMat->Get(AI_MATKEY_OPACITY, value) == AI_SUCCESS) 
    {
        opacity = value;
    }

    // Reflectivity
    if (aiMat->Get(AI_MATKEY_REFLECTIVITY, value) == AI_SUCCESS) 
    {
        reflectivity = value;
    }

    // Index of refraction
    if (aiMat->Get(AI_MATKEY_REFRACTI, value) == AI_SUCCESS) 
    {
        refractive_index = value;
    }

    // Get texture paths and load textures
    std::string tex_path;

    // Diffuse texture
    tex_path = getTexturePath(aiMat, aiTextureType_DIFFUSE);
    diffuse_texture = loadTexture(tex_path, modelDirectory);

    // Specular texture
    tex_path = getTexturePath(aiMat, aiTextureType_SPECULAR);
    specular_texture = loadTexture(tex_path, modelDirectory);

    // Normal texture
    tex_path = getTexturePath(aiMat, aiTextureType_NORMALS);
    normal_texture = loadTexture(tex_path, modelDirectory);

    // Height/bump texture
    tex_path = getTexturePath(aiMat, aiTextureType_HEIGHT);
    height_texture = loadTexture(tex_path, modelDirectory);
}


// Extract a texture path from a material
std::string phVkMaterial::getTexturePath(const aiMaterial* mat, aiTextureType type)
{
    if (mat->GetTextureCount(type) > 0)
    {
        aiString path;
        if (mat->GetTexture(type, 0, &path) == AI_SUCCESS)
        {
            return std::string(path.C_Str());
        }
    }
    return "";
}


// Function to load a texture from a file
std::unique_ptr<phVkTexture> phVkMaterial::loadTexture(
    const std::string& texturePath, const std::string& modelDirectory) 
{
    auto texture = std::make_unique<phVkTexture>();

    // Save the path for reference
    texture->path = texturePath;

    if (texturePath.empty()) 
    {
        return texture; // Empty path, return unloaded texture
    }

    // Construct full path
    std::filesystem::path fullPath;

    // Check if the texture path is absolute or relative
    std::filesystem::path texPath(texturePath);
    if (texPath.is_absolute()) 
    {
        fullPath = texPath;
    }
    else 
    {
        // If relative, combine with model directory
        fullPath = std::filesystem::path(modelDirectory) / texPath;
    }

    // Convert to string and normalize path
    std::string finalPath = fullPath.string();

    // Load the image using stb_image
    texture->data = stbi_load(
        finalPath.c_str(),
        &texture->width,
        &texture->height,
        &texture->channels,
        0  // 0 = auto-detect channels
    );

    if (texture->data) 
    {
        texture->isLoaded = true;
        std::cout << "Loaded texture: " << finalPath << " ("
            << texture->width << "x" << texture->height
            << ", " << texture->channels << " channels)" << std::endl;
    }
    else 
    {
        std::cerr << "Failed to load texture: " << finalPath << std::endl;
        std::cerr << "stbi_failure_reason: " << stbi_failure_reason() << std::endl;
    }

    return texture;
}
