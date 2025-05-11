// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Default Vulkan Initializer Structures

#include <vulkan/vulkan.h>

VkImageViewCreateInfo phVkDefaultImageViewCreateInfo()
{
    VkImageViewCreateInfo temp = {};

    temp.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    temp.pNext = nullptr;

    //temp.flags = 0;
    //temp.image = 0;
    temp.viewType = VK_IMAGE_VIEW_TYPE_2D;
    //temp.format = 0;
    //temp.compoonents = 0;
    temp.subresourceRange.baseMipLevel = 0;
    temp.subresourceRange.levelCount = 1;
    temp.subresourceRange.baseArrayLayer = 0;
    temp.subresourceRange.layerCount = 1;
    temp.subresourceRange.aspectMask = 0;

	return temp;
}

VkImageCreateInfo phVkDefaultImageCreateInfo()
{
	VkImageCreateInfo temp = {};

    temp.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    temp.pNext = nullptr;

    //temp.flags = 0;

    temp.imageType = VK_IMAGE_TYPE_2D;

    //temp.format = 0;
    //temp.extent = 0;

    temp.mipLevels = 1;
    temp.arrayLayers = 1;

    temp.samples = VK_SAMPLE_COUNT_1_BIT;

    temp.tiling = VK_IMAGE_TILING_OPTIMAL;
    //temp.usage = 0;

	//temp.sharingMode = 0;
	//temp.queueFamilyIndexCount = 0;
	//temp.pQueueFamilyIndices = 0;
	//temp.initialLayout = 0;

    return temp;
}

VkCommandPoolCreateInfo phVkDefaultCommandPoolCreateInfo()
{
	VkCommandPoolCreateInfo temp = {};

	temp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	temp.pNext = nullptr;
	//temp.flags = 0;
	temp.queueFamilyIndex = 0;

	return temp;
}

VkCommandBufferAllocateInfo phVkDefaultCommandBufferAllocateInfo()
{
	VkCommandBufferAllocateInfo temp = {};

	temp.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	temp.pNext = nullptr;
	//temp.commandPool = 0;
	temp.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	temp.commandBufferCount = 0;

	return temp;
}

VkPipelineShaderStageCreateInfo phvkDefaultPipelineShader()
{
	VkPipelineShaderStageCreateInfo temp = {};

	temp.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	temp.pNext = nullptr;
	//temp.flags = 0;
	temp.stage = VK_SHADER_STAGE_ALL;
	temp.module = 0;
	temp.pName = nullptr;
	temp.pSpecializationInfo = nullptr;
	
	return temp;
}

VkPipelineShaderStageCreateInfo phvkDefaultShaderStageCreateInfo()
{
	VkPipelineShaderStageCreateInfo temp = {};

    temp.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    temp.pNext = nullptr;
    //temp.flags;
    //temp.stage;
    //temp.module;
    //temp.pName;
    //temp.pSpecializationInfo;

    return temp;
}
