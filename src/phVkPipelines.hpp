// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Pipeline management

#pragma once

#include <vulkan/vulkan.h>

#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <string>

#include "array_list.hpp"

#include "phVkTypes.h"
#include "phVkInitDefaults.h"

// TODO: Re-write this with inheritance for different pipelines? But there are only three?

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
        throw std::runtime_error("Failed to open file: " + std::string(file_path));

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
        throw std::runtime_error("Error reading file: " + 
			std::string(file_path) + ", only " +
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
		throw std::runtime_error("Failed to create shader module for file: " + 
			std::string(file_path));
		return false;
	}
	else
		return true;
}


class phVkPipeline
{
public:
	// TODO: make some member data private?

	// -- Member Data --
	//
    phVkPipelineType type;
    VkDevice device;
	VkViewport viewport;
	VkRect2D scissor;

	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkPipelineLayoutCreateInfo layout_create_info;
	
	// Push constants
	ArrayList<VkPushConstantRange> push_constant_ranges;

	// Descriptor sets
	ArrayList<VkDescriptorSetLayout> descriptor_set_layouts;

	// Shader modules
	int compute_shader_index;
	int vertex_shader_index;
	int fragment_shader_index;
	ArrayList<VkPipelineShaderStageCreateInfo> shader_stages;

	// Formats
	ArrayList<VkFormat> color_attachment_formats;
	
	// Pipeline build data
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
	VkPipelineRasterizationStateCreateInfo rasterizer_info;
	VkPipelineMultisampleStateCreateInfo multisampling_info;
	VkPipelineRenderingCreateInfo render_info;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info;
	VkPipelineColorBlendAttachmentState color_blend_attachment;



	// -- Member Functions --
	//
	phVkPipeline();
	phVkPipeline(VkDevice device, phVkPipelineType type, VkViewport viewport, VkRect2D scissor);
	~phVkPipeline();

	// Setup
	void setType(phVkPipelineType type);
	void setDevice(VkDevice device);

	// Viewport / Scissor
	void setViewportScissor(VkViewport viewport, VkRect2D scissor);

    // Push constants
   	void addPushConstantRange(VkPushConstantRange push_constant_range);

    // Descriptor sets
	void addDescriptorSetLayout(VkDescriptorSetLayout descriptor_set_layout);

    // Shaders
    void setComputeShader(VkShaderModule cs, const char* entry_function = "main");
	void setVertexShader(VkShaderModule vs);
	void setFragmentShader(VkShaderModule fs);
    void setGraphicsShaders(VkShaderModule vs, VkShaderModule fs);
	bool loadComputeShader(const char* file_path, const char* entry_function = "main");
	bool loadVertexShader(const char* file_path);
	bool loadFragmentShader(const char* file_path);

    // Geometry
	void setInputTopology(VkPrimitiveTopology topology, VkBool32 primitive_restart = VK_FALSE);
	void setPolygonMode(VkPolygonMode mode, float line_width = 1.f);
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
	void enableDepthTest(bool depth_write_enable, VkCompareOp op, float min = 0.f, float max = 1.f);

	// Image format
	void setDepthFormat(VkFormat format);
	void addColorAttachmentFormat(VkFormat format);

	// Pipeline
	//void setLayoutFlags();
    bool createLayout();    // Called by createPipeline
    bool createPipeline();

	// Data Management
	void clearToDefaults();
	void destroyShaderModules();
};


phVkPipeline::phVkPipeline() 
{ 
	clearToDefaults(); 
}


phVkPipeline::phVkPipeline(VkDevice device, phVkPipelineType type, 
	VkViewport viewport, VkRect2D scissor)
{
	phVkPipeline();		// calls clearToDefaults()

	setType(type);
	setDevice(device);
	setViewportScissor(viewport, scissor);
}


phVkPipeline::~phVkPipeline() 
{ 
	clearToDefaults();
}


void phVkPipeline::setType(phVkPipelineType type) 
{ 
	this->type = type; 
}


void phVkPipeline::setDevice(VkDevice device) 
{ 
	this->device = device; 
}


void phVkPipeline::setViewportScissor(VkViewport viewport, VkRect2D scissor) 
{ 
	this->viewport = viewport; 
	this->scissor = scissor; 
}


void phVkPipeline::addPushConstantRange(VkPushConstantRange push_constant_range)
{
	push_constant_ranges.push(push_constant_range);

	layout_create_info.pushConstantRangeCount = push_constant_ranges.getCount();
	layout_create_info.pPushConstantRanges = &push_constant_ranges.getData();
}


void phVkPipeline::addDescriptorSetLayout(VkDescriptorSetLayout descriptor_set_layout)
{
	descriptor_set_layouts.push(descriptor_set_layout);

	layout_create_info.setLayoutCount = descriptor_set_layouts.getCount();
	layout_create_info.pSetLayouts = &descriptor_set_layouts.getData();
}


void phVkPipeline::setComputeShader(VkShaderModule cs, const char* entry_function)
{
	VkPipelineShaderStageCreateInfo temp_create_info = phvkDefaultShaderStageCreateInfo();
	temp_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	temp_create_info.pName = entry_function;
	temp_create_info.module = cs;
	shader_stages.push(temp_create_info);
}


void phVkPipeline::setVertexShader(VkShaderModule vs)
{
	VkPipelineShaderStageCreateInfo temp_create_info = phvkDefaultShaderStageCreateInfo();
	temp_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	temp_create_info.module = vs;
	shader_stages.push(temp_create_info);
}


void phVkPipeline::setFragmentShader(VkShaderModule fs)
{
	VkPipelineShaderStageCreateInfo temp_create_info = phvkDefaultShaderStageCreateInfo();
	temp_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	temp_create_info.module = fs;
	shader_stages.push(temp_create_info);
}


void phVkPipeline::setGraphicsShaders(VkShaderModule vs, VkShaderModule fs)
{
	setVertexShader(vs);
	setFragmentShader(fs);
}


bool phVkPipeline::loadComputeShader(const char* file_path, const char* entry_function = "main")
{
	VkPipelineShaderStageCreateInfo temp_create_info = phvkDefaultShaderStageCreateInfo();
	temp_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	temp_create_info.pName = entry_function;
	unsigned int stage_index = shader_stages.push(temp_create_info);

	if (!loadShaderModule(file_path, device, &shader_stages[stage_index].module))
	{
		vkDestroyShaderModule(device, shader_stages[stage_index].module, nullptr);
		shader_stages.remove(stage_index);
		return false;
	}

	return true;
}


bool phVkPipeline::loadVertexShader(const char* file_path)
{
	VkPipelineShaderStageCreateInfo temp_create_info = phvkDefaultShaderStageCreateInfo();
	temp_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	unsigned int stage_index = shader_stages.push(temp_create_info);

	if (!loadShaderModule(file_path, device, &shader_stages[stage_index].module))
	{
		vkDestroyShaderModule(device, shader_stages[stage_index].module, nullptr);
		shader_stages.remove(stage_index);
		return false;
	}

	return true;
}


bool phVkPipeline::loadFragmentShader(const char* file_path)
{
	VkPipelineShaderStageCreateInfo temp_create_info = phvkDefaultShaderStageCreateInfo();
	temp_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	unsigned int stage_index = shader_stages.push(temp_create_info);

	if (!loadShaderModule(file_path, device, &shader_stages[stage_index].module))
	{
		vkDestroyShaderModule(device, shader_stages[stage_index].module, nullptr);
		shader_stages.remove(stage_index);
		return false;
	}

	return true;
}


void phVkPipeline::setInputTopology(VkPrimitiveTopology topology, VkBool32 primitive_restart)
{
	input_assembly_info.topology = topology;
	input_assembly_info.primitiveRestartEnable = primitive_restart;
}


void phVkPipeline::setPolygonMode(VkPolygonMode mode, float line_width)
{
	rasterizer_info.polygonMode = mode;
	rasterizer_info.lineWidth = line_width;
}


void phVkPipeline::setCullMode(VkCullModeFlags cull_mode, VkFrontFace front_face)
{
	rasterizer_info.cullMode = cull_mode;
	rasterizer_info.frontFace = front_face;
}


void phVkPipeline::setMultiSamplingNone()
{
	multisampling_info.sampleShadingEnable = VK_FALSE;
	multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling_info.minSampleShading = 1.0f;
	multisampling_info.pSampleMask = nullptr;
	multisampling_info.alphaToCoverageEnable = VK_FALSE;
	multisampling_info.alphaToOneEnable = VK_FALSE;
}


void phVkPipeline::disableBlending()
{
	color_blend_attachment.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
}


void phVkPipeline::enableBlendingAdd()
{
	color_blend_attachment.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}


void phVkPipeline::enableBlendingAlpha()
{
	color_blend_attachment.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}


void phVkPipeline::disableDepthTest()
{
	depth_stencil_info.depthTestEnable = VK_FALSE;
	depth_stencil_info.depthWriteEnable = VK_FALSE;
	depth_stencil_info.depthCompareOp = VK_COMPARE_OP_NEVER;
	depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_info.stencilTestEnable = VK_FALSE;
	depth_stencil_info.front = {};
	depth_stencil_info.back = {};
	depth_stencil_info.minDepthBounds = 0.f;
	depth_stencil_info.maxDepthBounds = 1.f;
}


void phVkPipeline::enableDepthTest(bool depth_write_enable, VkCompareOp op, float min, float max)
{
	depth_stencil_info.depthTestEnable = VK_TRUE;
	depth_stencil_info.depthWriteEnable = depth_write_enable;
	depth_stencil_info.depthCompareOp = op;
	depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_info.stencilTestEnable = VK_FALSE;
	depth_stencil_info.front = {};
	depth_stencil_info.back = {};
	depth_stencil_info.minDepthBounds = min;
	depth_stencil_info.maxDepthBounds = max;
}


void phVkPipeline::setDepthFormat(VkFormat format)
{
	render_info.depthAttachmentFormat = format;
}


void phVkPipeline::addColorAttachmentFormat(VkFormat format)
{
	color_attachment_formats.push(format);

	render_info.colorAttachmentCount = color_attachment_formats.getCount();
	render_info.pColorAttachmentFormats = &color_attachment_formats.getData();
}


bool phVkPipeline::createLayout()
{
	if (layout)
		return false;

	VK_CHECK(vkCreatePipelineLayout(device, &layout_create_info, nullptr, &layout));

	return true;
}


bool phVkPipeline::createPipeline()
{
	if (pipeline)
		return false;

	// Viewport state
	// TODO: add support for multiple viewports/scissors
	VkPipelineViewportStateCreateInfo viewport_state_info = {};
	viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_info.pNext = nullptr;
	//viewport_state_info.flags;
	viewport_state_info.viewportCount = 1;	// TODO: add support for multiple
	viewport_state_info.pViewports = &viewport;
	viewport_state_info.scissorCount = 1;	// TODO: add support for multiple
	viewport_state_info.pScissors = &scissor;

	// Color blending
	VkPipelineColorBlendStateCreateInfo color_blending_info = {};
	color_blending_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending_info.pNext = nullptr;
	//color_blending_info.flags;
	color_blending_info.logicOpEnable = VK_FALSE;	// TODO: implement for transparent
	color_blending_info.logicOp = VK_LOGIC_OP_COPY;
	color_blending_info.attachmentCount = 1;		// TODO: add support for multiple
	color_blending_info.pAttachments = &color_blend_attachment;
	//color_blending_info.blendConstants[4];

	// Vertex input state
	// TODO: when is this needed?
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	//vertex_input_info.pNext = nullptr;
	//vertex_input_info.flags = 0;
	//vertex_input_info.vertexBindingDescriptionCount = 0;
	//vertex_input_info.pVertexBindingDescriptions = nullptr;
	//vertex_input_info.vertexAttributeDescriptionCount = 0;
	//vertex_input_info.pVertexAttributeDescriptions = nullptr;

	// Dynamic rendering
	// TODO: update to support multiple
	VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamic_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic_info.pDynamicStates = &state[0];
	dynamic_info.dynamicStateCount = 2;

	// Pipeline create info
	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.pNext = &render_info;	// Connect render info extension
	//pipeline_info.flags = 0;
	pipeline_info.stageCount = (uint32_t)shader_stages.getCount();
	pipeline_info.pStages = shader_stages.getData();
	pipeline_info.pVertexInputState = &vertex_input_info;
	pipeline_info.pInputAssemblyState = &input_assembly_info;
	//pipeline_info.pTessellationState = nullptr;
	pipeline_info.pViewportState = &viewport_state_info;
	pipeline_info.pRasterizationState = &rasterizer_info;
	pipeline_info.pMultisampleState = &multisampling_info;
	pipeline_info.pDepthStencilState = &depth_stencil_info;
	pipeline_info.pColorBlendState = &color_blending_info;
	pipeline_info.pDynamicState = &dynamic_info;
	pipeline_info.layout = layout;
	//pipeline_info.renderPass = 0;
	//pipeline_info.subpass = 0;
	//pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	//pipeline_info.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(
		device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline)
		!= VK_SUCCESS) 
	{
		std::cout << "Failed to create pipeline" << std::endl;
		return false;
	}
	else 
	{
		return true;
	}
}


void phVkPipeline::clearToDefaults()
{
	destroyShaderModules();
	
	if (layout)
		vkDestroyPipelineLayout(device, layout, nullptr);

	if (pipeline)
		vkDestroyPipeline(device, pipeline, nullptr);

	type = VULKAN_PIPELINE_NONE;
	device = 0;
	viewport = VkViewport{};
	scissor = VkRect2D{};

	pipeline = VK_NULL_HANDLE;
	layout = VK_NULL_HANDLE;
	layout_create_info = {};
	layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	push_constant_ranges.clear();

	descriptor_set_layouts.clear();

	compute_shader_index = -1;
	vertex_shader_index = -1;
	fragment_shader_index = -1;

	color_attachment_formats.clear();

	input_assembly_info = {};
	input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

	rasterizer_info = {};
	rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	multisampling_info = {};
	multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

	render_info = {};
	render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

	depth_stencil_info = {};
	depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	color_blend_attachment = {};
}


void phVkPipeline::destroyShaderModules()
{
	for (int i = 0; i < shader_stages.getCount(); i++)
	{
		vkDestroyShaderModule(device, shader_stages[i].module, nullptr);
	}

	shader_stages.clear();
}


