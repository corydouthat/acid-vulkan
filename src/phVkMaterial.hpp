// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Material and texture classes

#pragma once

#include <iostream>
#include <filesystem>
#include <cstdio>
#include <cstring>

#include <vulkan/vulkan.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "phVkEngine.hpp"
#include "phVkImages.hpp"

//#include <stb_image.h>    // Only include once?

#include "array_list.hpp"

#include "vec.hpp"
#include "mat.hpp"


template <typename T>
class phVkEngine;


// Texture data structure
class phVkTexture 
{
public:
    unsigned char* data = nullptr;

    std::string path;

    bool is_loaded = false;

    int width = 0;
    int height = 0;
    int channels = 0;

    phVkTexture() = default;
    phVkTexture(const phVkTexture&) = delete;   // Disable copy constructor
    ~phVkTexture();
    const phVkTexture& operator=(const phVkTexture& other);

    // Functions
    bool loadTexture(const std::string& texture_path, const std::string& model_directory);
};


phVkTexture::~phVkTexture()
{
    if (data)
    {
        stbi_image_free(data);
        data = nullptr;
    }
}

const phVkTexture& phVkTexture::operator=(const phVkTexture& other)
{
    path = other.path;

    is_loaded = other.is_loaded;

    width = other.width;
    height = other.height;
    channels = other.channels;

    if (is_loaded)
        memcpy(data, other.data, width * height * channels);
    else
        data = nullptr;

    return *this;
}


// Function to load a texture from a file
bool phVkTexture::loadTexture(
    const std::string& texture_path, const std::string& model_directory)
{
    // Save the path for reference
    path = texture_path;

    if (texture_path.empty())
        return false;

    // Construct full path
    std::filesystem::path full_path;

    // Check if the texture path is absolute or relative
    std::filesystem::path tex_path(texture_path);
    if (tex_path.is_absolute())
    {
        full_path = tex_path;
    }
    else
    {
        // If relative, combine with model directory
        full_path = std::filesystem::path(model_directory) / tex_path;
    }

    // Convert to string and normalize path
    std::string final_path = full_path.string();

    // Load the image using stb_image
    data = stbi_load(
        final_path.c_str(),
        &width,
        &height,
        &channels,
        0  // 0 = auto-detect channels
    );

    if (data)
    {
        is_loaded = true;
        std::cout << "Loaded texture: " << final_path << " ("
            << width << "x" << height
            << ", " << channels << " channels)" << std::endl;

        return true;
    }
    else
    {
        std::cerr << "Failed to load texture: " << final_path << std::endl;
        std::cerr << "stbi_failure_reason: " << stbi_failure_reason() << std::endl;

        return false;
    }
}


template<typename T = float>    // Only needed for phVkEngine pointer type?
class phVkMaterial
{
public:
    std::string name;		// Human friendly name (optional)
    std::string file_path;	// Original file path

    phVkEngine<T>* engine;

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
    phVkTexture diffuse_texture;
    phVkTexture specular_texture;
    phVkTexture normal_texture;
    phVkTexture height_texture;

    // Default constructor with some reasonable values
    phVkMaterial() 
    {
        engine = nullptr;

        // Initialize with default values
        diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 0.8f; diffuse_color[3] = 1.0f;
        specular_color[0] = specular_color[1] = specular_color[2] = 0.0f; specular_color[3] = 1.0f;
        ambient_color[0] = ambient_color[1] = ambient_color[2] = 0.2f; ambient_color[3] = 1.0f;
        emissive_color[0] = emissive_color[1] = emissive_color[2] = 0.0f; emissive_color[3] = 1.0f;

        shininess = 0.0f;
        opacity = 1.0f;
        reflectivity = 0.0f;
        refractive_index = 1.0f;
    }

    // Functions
    void processMaterial(const aiMaterial* mat, const aiScene* scene, 
        const std::string& model_directory);
    std::string getTexturePath(const aiMaterial* mat, aiTextureType type);
    phVkTexture loadTexture(const std::string& texture_path, 
        const std::string& model_directory);

    void initVulkan(phVkEngine<T>* engine);
    void vulkanCleanup();
};


// Main function to process material data from a loaded model
template<typename T>
void phVkMaterial<T>::processMaterial(const aiMaterial* mat, const aiScene* scene, 
    const std::string& model_directory)
{
    // Get material name
    aiString name;
    if (mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) 
    {
        this->name = name.C_Str();
        std::cout << "Material: " << name.C_Str() << std::endl;
    }

    // Get material colors
    aiColor4D color;

    // Diffuse color
    if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) 
    {
        diffuse_color[0] = color.r;
        diffuse_color[1] = color.g;
        diffuse_color[2] = color.b;
        diffuse_color[3] = color.a;
    }

    // Specular color
    if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) 
    {
        specular_color[0] = color.r;
        specular_color[1] = color.g;
        specular_color[2] = color.b;
        specular_color[3] = color.a;
    }

    // Ambient color
    if (mat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) 
    {
        ambient_color[0] = color.r;
        ambient_color[1] = color.g;
        ambient_color[2] = color.b;
        ambient_color[3] = color.a;
    }

    // Emissive color
    if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) 
    {
        emissive_color[0] = color.r;
        emissive_color[1] = color.g;
        emissive_color[2] = color.b;
        emissive_color[3] = color.a;
    }

    // Get material properties
    float value;

    // Shininess
    if (mat->Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS) 
    {
        shininess = value;
    }

    // Opacity
    if (mat->Get(AI_MATKEY_OPACITY, value) == AI_SUCCESS) 
    {
        opacity = value;
    }

    // Reflectivity
    if (mat->Get(AI_MATKEY_REFLECTIVITY, value) == AI_SUCCESS) 
    {
        reflectivity = value;
    }

    // Index of refraction
    if (mat->Get(AI_MATKEY_REFRACTI, value) == AI_SUCCESS) 
    {
        refractive_index = value;
    }

    // Get texture paths and load textures
    std::string tex_path;

    // Diffuse texture
    tex_path = getTexturePath(mat, aiTextureType_DIFFUSE);
    diffuse_texture.loadTexture(tex_path, model_directory);

    // Specular texture
    tex_path = getTexturePath(mat, aiTextureType_SPECULAR);
    specular_texture.loadTexture(tex_path, model_directory);

    // Normal texture
    tex_path = getTexturePath(mat, aiTextureType_NORMALS);
    normal_texture.loadTexture(tex_path, model_directory);

    // Height/bump texture
    tex_path = getTexturePath(mat, aiTextureType_HEIGHT);
    height_texture.loadTexture(tex_path, model_directory);
}


// Extract a texture path from a material
template<typename T>
std::string phVkMaterial<T>::getTexturePath(const aiMaterial* mat, aiTextureType type)
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


template <typename T>
void phVkMaterial<T>::initVulkan(phVkEngine<T>* engine)
{
    this->engine = engine;

    // TODO
}


template <typename T>
void phVkMaterial<T>::vulkanCleanup()
{
    // TODO
}
