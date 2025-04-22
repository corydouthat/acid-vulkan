// Written by : Patrick Marsceill (vkguide.dev)
// Used with permission - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Pipelines boilerplate

#pragma once

#include "phvk_types.h"

class PipelineBuilder {
//> pipeline
public:
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
   
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    VkPipelineMultisampleStateCreateInfo multi_sampling;
    VkPipelineLayout pipeline_layout;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineRenderingCreateInfo render_info;
    VkFormat color_attachment_format;

	PipelineBuilder(){ clear(); }

    void clear();

    VkPipeline buildPipeline(VkDevice device);
//< pipeline
    void setShaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader);
    void setInputTopology(VkPrimitiveTopology topology);
    void setPolygonMode(VkPolygonMode mode);
    void setCullMode(VkCullModeFlags cull_mode, VkFrontFace front_face);
    void setMultiSamplingNone();
    void disableBlending();
    void enableBlendingAdditive();
    void enableBlendingAlphaBlend();

    void setColorAttachmentFormat(VkFormat format);
	void setDepthFormat(VkFormat format);
	void disableDepthtest();
    void enableDepthtest(bool depth_write_enable, VkCompareOp op);
};

namespace vkutil 
{
    bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}
