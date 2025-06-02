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
#include "phVkDescriptors.hpp"

//#include <stb_image.h>    // Only include once?

#include "array_list.hpp"

#include "vec.hpp"
#include "mat.hpp"


template <typename T>
class phVkEngine;


// Texture data structure
template <typename T>   // Only for phVkEngine
class phVkTexture
{
public:
    // Meta-data
    std::string path;
    bool is_loaded = false;
    bool vulkan_initialized = false;
    int width = 0;
    int height = 0;
    int channels = 0;

    // Data
    unsigned char* data = nullptr;

    // Vulkan
    phVkEngine<T>* engine = nullptr;
    phVkImage vulkan_image = {};  // Vulkan image data struct
    VkSampler vulkan_sampler = VK_NULL_HANDLE; // Vulkan sampler object

    phVkTexture();
    ~phVkTexture();
    phVkTexture(const phVkTexture&) = delete;   // Disable copy constructor
    const phVkTexture& operator=(const phVkTexture& other);

    // Functions
    bool loadTexture(const aiScene* scene, const std::string& texture_path, 
        const std::string& model_directory);
    void initVulkan(phVkEngine<T>* engine);
    void vulkanCleanup();
};

template <typename T>
phVkTexture<T>::phVkTexture()
{
    bool is_loaded = false;
    bool vulkan_initialized = false;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = nullptr;
    phVkEngine<T>* engine = nullptr;
    phVkImage vulkan_image = {};
    VkSampler vulkan_sampler = VK_NULL_HANDLE;
}


template <typename T>
phVkTexture<T>::~phVkTexture()
{
    vulkanCleanup();

    if (data)
    {
        stbi_image_free(data);
        data = nullptr;
    }
}

template <typename T>
const phVkTexture<T>& phVkTexture<T>::operator=(const phVkTexture& other)
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

    // TODO: copy Vulkan data

    return *this;
}


// Function to load a texture from a file
template <typename T>
bool phVkTexture<T>::loadTexture(const aiScene* scene,
    const std::string& texture_path, const std::string& model_directory)
{
    if (texture_path.empty())
        return false;

    // Save the path for reference
    path = texture_path;

    if (path.starts_with("*"))    // Embedded texture
    {
        const aiTexture* ai_tex = scene->GetEmbeddedTexture(texture_path.c_str());
		if (!ai_tex->pcData)
		{
			std::cerr << "Invalid embedded texture data." << std::endl;
			return false;
		}

        // Note: mHeight == 0 indicates compressed image

        // Automatically detects format?
        stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(ai_tex->pcData),
            ai_tex->mHeight > 0 ?
			ai_tex->mWidth * ai_tex->mHeight * 4 : // Raw image dimensions
            ai_tex->mWidth,  // Holds the size in bytes for embedded textures
			&width,
			&height,
			&channels,
			0  // 0 = auto-detect channels
		);
    }
    else                        // Load from file
    {
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

    is_loaded = true;
}


template <typename T>
void phVkTexture<T>::initVulkan(phVkEngine<T>* engine)
{
    if (!is_loaded || !engine)
        return;

    this->engine = engine;

    // Determine format based on channels
    VkFormat format;
    switch (channels) 
    {
    case 1: 
        format = VK_FORMAT_R8_UNORM; break;
    case 2: 
        format = VK_FORMAT_R8G8_UNORM; break;
    case 3: 
        format = VK_FORMAT_R8G8B8_UNORM; break;
    case 4: 
        format = VK_FORMAT_R8G8B8A8_UNORM; break;
	default: 
        std::cout << "Unsupported texture format with "
		<< channels << " channels." << std::endl;
		throw std::runtime_error("Unsupported texture format.");
		return;
    }

	VkExtent3D extent = 
    {
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height),
		1
	};

    // Create image
	vulkan_image = engine->createImage(
		data, extent, format,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    // Create sampler
    // TODO: make configurable
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    //sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    //sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    //sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    //sampler_info.anisotropyEnable = VK_TRUE;
    //sampler_info.maxAnisotropy = 16.0f;
    //sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    //sampler_info.unnormalizedCoordinates = VK_FALSE;
    //sampler_info.compareEnable = VK_FALSE;
    //sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    //sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(engine->device, &sampler_info, nullptr, &vulkan_sampler));

	vulkan_initialized = true;
}


template <typename T>
void phVkTexture<T>::vulkanCleanup()
{
	if (vulkan_initialized)
	{
		if (vulkan_sampler)
		{
			vkDestroySampler(engine->device, vulkan_sampler, nullptr);
			vulkan_sampler = VK_NULL_HANDLE;
		}
		
        if (vulkan_image.image)
		{
			engine->destroyImage(vulkan_image);
            vulkan_image = {};
		}

		vulkan_initialized = false;
	}
}


struct phVkMaterialDataFull
{
    alignas(16) float diffuse_color[4];
    alignas(16) float specular_color[4];
    alignas(16) float ambient_color[4];
    alignas(16) float emissive_color[4];
    alignas(4) float shininess;
    alignas(4) float opacity;
    alignas(4) float reflectivity;
    alignas(4) float refractive_index;
    alignas(4) bool has_diffuse_texture;
    alignas(4) bool has_specular_texture;
    alignas(4) bool has_normal_texture;
    alignas(4) bool has_height_texture;
};


struct phVkMaterialDataVkGuide
{
    alignas(16) float diffuse_color[4];
    alignas(16) float specular_color[4];
};


template<typename T = float>    // Only needed for phVkEngine pointer type?
class phVkMaterial
{
public:
    std::string name;		// Human friendly name (optional)
    std::string file_path;	// Original file path

	bool vulkan_initialized = false; // Vulkan initialization flag

    phVkEngine<T>* engine;

    // Material light colors
    float diffuse_color[4];
    float specular_color[4];
    float ambient_color[4];
    float emissive_color[4];

    // Material properties
    float shininess;
    float opacity;
    float reflectivity;
    float refractive_index;

    // Texture flags
    bool has_diffuse_texture;
    bool has_specular_texture;
    bool has_normal_texture;
    bool has_height_texture;

    // Texture objects
    phVkTexture<T> diffuse_texture;
    phVkTexture<T> specular_texture;
    phVkTexture<T> normal_texture;
    phVkTexture<T> height_texture;

    // Vulkan objects
	AllocatedBuffer material_buffer;  // Buffer for material data
    VkDescriptorSet material_descriptor_set;

    // Default constructor with some reasonable values
    phVkMaterial() 
    {
        engine = nullptr;

        // Initialize with default values
        diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0f; diffuse_color[3] = 1.0f;
        specular_color[0] = specular_color[1] = specular_color[2] = 1.0f; specular_color[3] = 1.0f;
        ambient_color[0] = ambient_color[1] = ambient_color[2] = 1.0f; ambient_color[3] = 1.0f;
        emissive_color[0] = emissive_color[1] = emissive_color[2] = 0.0f; emissive_color[3] = 1.0f;

        shininess = 0.0f;
        opacity = 1.0f;
        reflectivity = 0.0f;
        refractive_index = 2.0f;

        vulkan_initialized = false;

        material_buffer = {};
    }

    ~phVkMaterial()
    {
        vulkanCleanup();
    }

    // Functions
    void processMaterial(const aiMaterial* mat, const aiScene* scene, 
        const std::string& model_directory);
    std::string getTexturePath(const aiMaterial* mat, aiTextureType type);

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
    diffuse_texture.loadTexture(scene, tex_path, model_directory);

    // Specular texture
    tex_path = getTexturePath(mat, aiTextureType_SPECULAR);
    specular_texture.loadTexture(scene, tex_path, model_directory);

    // Normal texture
    tex_path = getTexturePath(mat, aiTextureType_NORMALS);
    normal_texture.loadTexture(scene, tex_path, model_directory);

    // Height/bump texture
    tex_path = getTexturePath(mat, aiTextureType_HEIGHT);
    height_texture.loadTexture(scene, tex_path, model_directory);
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
    if (!engine)
        return;

    this->engine = engine;

    // TODO: could pre-organize data and in phVkMaterial and copy directly 
    //       if we settle on a single structure?

    // Choose material data structure for GPU
	phVkMaterialDataVkGuide material_data;

    // Copy material light colors to GPU structure
    memcpy(material_data.diffuse_color, diffuse_color, sizeof(diffuse_color));
    memcpy(material_data.specular_color, specular_color, sizeof(specular_color));
    //memcpy(material_data.ambient_color, ambient_color, sizeof(ambient_color));
    //memcpy(material_data.emissive_color, emissive_color, sizeof(emissive_color));

    // Copy material properties
    //material_data.shininess = shininess;
    //material_data.opacity = opacity;
    //material_data.reflectivity = reflectivity;
    //material_data.refractive_index = refractive_index;

    // Set texture availability flags
    //material_data.has_diffuse_texture = diffuse_texture.is_loaded ? 1 : 0;
    //material_data.has_specular_texture = specular_texture.is_loaded ? 1 : 0;
    //material_data.has_normal_texture = normal_texture.is_loaded ? 1 : 0;
    //material_data.has_height_texture = height_texture.is_loaded ? 1 : 0;


    // Create Vulkan images for textures
    if (diffuse_texture.is_loaded) {
        diffuse_texture.initVulkan(engine);
    }

    if (specular_texture.is_loaded) {
        specular_texture.initVulkan(engine);
    }

    //if (normal_texture.is_loaded) {
    //    normal_texture.initVulkan(engine);
    //}

    //if (height_texture.is_loaded) {
    //    height_texture.initVulkan(engine);
    //}


    // Create material data buffer
    material_buffer = engine->createBuffer(
        sizeof(phVkMaterialDataVkGuide),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    // Set material data
	phVkMaterialDataVkGuide* material_data_ptr =
		(phVkMaterialDataVkGuide*)material_buffer.allocation->GetMappedData();
	memcpy(material_data_ptr, &material_data, sizeof(phVkMaterialDataVkGuide));

    // Allocate descriptor set for this material
    // TODO: does it make sense to define the layout in the engine, not here?
    //       should be based on pipeline?
    material_descriptor_set = engine->global_descriptor_allocator.allocate(
        engine->device, engine->material_data_descriptor_layout);

    // Write descriptor data
	phVkDescriptorWriter writer;
    writer.clear();
    writer.writeBuffer(0, material_buffer.buffer, sizeof(phVkMaterialDataVkGuide),
        0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.writeImage(1, diffuse_texture.vulkan_image.view, diffuse_texture.vulkan_sampler,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.writeImage(2, specular_texture.vulkan_image.view, specular_texture.vulkan_sampler,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.updateSet(engine->device, material_descriptor_set);

    vulkan_initialized = true;
}


template <typename T>
void phVkMaterial<T>::vulkanCleanup()
{
    if (vulkan_initialized)
    {
		engine->destroyBuffer(material_buffer);

        // Descriptor set destroyed when engine pool is destroyed?

        vulkan_initialized = false;
    }
}
