// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Pipeline management

#pragma once

#include <vulkan/vulkan.h>

#include <fstream>
#include <vector>
#include <stdexcept>
#include <stdint>
#include <string>

#include "array_list.hpp"

#include "phVkTypes.h"
#include "phVkInitDefaults.h"

// TODO: Re-write this with inheritance for different pipelines

enum class phVkPipelineType
{
	VULKAN_PIPELINE_NONE = 0,
	VULKAN_COMPUTE_PIPELINE = 1,
	VULKAN_GRAPHICS_PIPELINE = 2,
	//VULKAN_RAY_TRACING_PIPELINE = 3,
};


bool loadShaderModule(const char* file_path, VkDevice device, VkShaderModule* shader_module)
{
    // Open the file in binary mode
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);

    // Check if the file was opened successfully
    if (!file.is_open()) 
	{
        throw std::runtime_error("Failed to open file: " + file_path);

		return false;
    }

    // Get the file size
    std::streamsize file_size = file.tellg();

    // Calculate how many uint32_t values we need
    size_t num_elements = file_size / sizeof(uint32_t);

    // Create a vector to hold the data
    std::vector<uint32_t> buffer(num_elements);

    // Move to the beginning of the file
    file.seekg(0, std::ios::beg);

    // Read the entire file into the buffer
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    // Check if all data was read successfully
    if (!file) 
	{
        throw std::runtime_error("Error reading file: " file_path + ", only " +
            std::to_string(file.gcount()) +
            " bytes could be read");
    }

	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.codeSize = buffer.size() * sizeof(uint32_t);
	create_info.pCode = buffer.data();

	if (vkCreateShaderModule(device, &create_info, nullptr, shader_module) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module for file: " + file_path);
		return false;
	}
	else
		return true;
}


class phVkPipeline
{
public:
	// TODO: make some member data private?

    phVkPipelineType type = VULKAN_PIPELINE_NONE;
    VkDevice device = 0;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	VkPushConstantRange push_constant_range;
	ArrayList<VkDescriptorSetLayout> descriptor_set_layouts;

	VkShaderModule compute_shader = 0;
	VkShaderModule vertex_shader = 0;
	VkShaderModule fragment_shader = 0;
	std::string compute_shader_entry;

	// TODO: add others

	phVkPipeline(VkDevice d, phVkPipelineType t) : device(d), type(t) { ; }
    ~phVkPipeline() { clear(); }

    // Push constants
   	void addPushConstantRange(VkShaderStageFlags stage_flags, uint32_t offset, uint32_t size);

    // Descriptor sets
	void addDescriptorSetLayout(VkDescriptorSetLayout layout);

    // Shaders
    void setComputeShader(VkShaderModule cs, const char* entry_function = "main");
	void setVertexShader(VkShaderModule vs);
	void setFragmentShader(VkShaderModule fs);
    void setGraphicsShaders(VkShaderModule vs, VkShaderModule fs);
	bool loadComputeShader(const char* file_path, const char* entry_function = "main");
	bool loadVertexShader(const char* file_path);
	bool loadFragmentShader(const char* file_path);


    // Geometry
	void setInputTopology(VkPrimitiveTopology topology);
	void setPolygonMode(VkPolygonMode mode);
	void setCullMode(VkCullModeFlags cull_mode, VkFrontFace front_face);

	// Sampling
	void setMultiSamplingNone();
    //void setMultisampling();

	// Blending
	void disableBlending();
	void enableBlendingAdd();
	void enableBlendingAlpha();

	// Depth testing
	void disableDepthTest();
	void enableDepthTest(bool depth_write_en, VkCompareOp op);

	// Image format
	void setDepthFormat(VkFormat format);
	void addColorAttachmentFormat(VkFormat format);

	// Pipeline
	//void setLayoutFlags();
    void createLayout();    // Called by createPipeline
    void createPipeline();

	// Cleanup
	void clear();
	void destroyShaderModules();
};


void phVkPipeline::addDescriptorSetLayout(VkDescriptorSetLayout layout)
{
	descriptor_set_layouts.push_back(layout);
}


void phVkPipeline::setComputeShader(VkShaderModule cs, const char* entry_function = "main")
{
	compute_shader = cs;

	compute_shader_entry = entry_function;
}


void phVkPipeline::setVertexShader(VkShaderModule vs)
{
	vertex_shader = vs;
}


void phVkPipeline::setFragmentShader(VkShaderModule fs)
{
	fragment_shader = fs;
}


void phVkPipeline::setGraphicsShaders(VkShaderModule vs, VkShaderModule fs)
{
	vertex_shader = vs;
	fragment_shader = fs;
}


bool phVkPipeline::loadComputeShader(const char* file_path, const char* entry_function = "main")
{
	return loadShaderModule(file_path, device, &compute_shader)
	{
		vkDestroyShaderModule(device, compute_shader, nullptr);
		return false;
	}

	compute_shader_entry = entry_function;

	return true;
}


bool phVkPipeline::loadVertexShader(const char* file_path)
{
	if (!loadShaderModule(file_path, device, &vertex_shader))
	{
		vkDestroyShaderModule(device, vertex_shader, nullptr);
		return false;
	}

	return true;
}


bool phVkPipeline::loadFragmentShader(const char* file_path)
{
	return loadShaderModule(file_path, device, &fragment_shader)
	{
		vkDestroyShaderModule(device, fragment_shader, nullptr);
		return false;
	}

	return true;
}


void phVkPipeline::setInputTopology(VkPrimitiveTopology topology)
{

}


void phVkPipeline::setPolygonMode(VkPolygonMode mode)
{

}


void phVkPipeline::setCullMode(VkCullModeFlags cull_mode, VkFrontFace front_face)
{

}


void phVkPipeline::setMultiSamplingNone()
{

}


void phVkPipeline::disableBlending()
{

}


void phVkPipeline::enableBlendingAdd()
{

}


void phVkPipeline::enableBlendingAlpha()
{

}


void phVkPipeline::disableDepthTest()
{

}


void phVkPipeline::enableDepthTest(bool depth_write_en, VkCompareOp op)
{

}


void phVkPipeline::setDepthFormat(VkFormat format)
{

}


void phVkPipeline::addColorAttachmentFormat(VkFormat format)
{

}


