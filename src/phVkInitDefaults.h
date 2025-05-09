// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Default Vulkan Initializer Structures

#include <vulkan/vulkan.h>

VkImageViewCreateInfo phvk_default_image_view_create_info
{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    .pNext = nullptr;

	.flags = 0;
    .image = 0;
    .viewType = VK_IMAGE_VIEW_TYPE_2D;
    .format = 0;
    .compoonents = 0;
    .subresourceRange.baseMipLevel = 0;
    .subresourceRange.levelCount = 1;
    .subresourceRange.baseArrayLayer = 0;
    .subresourceRange.layerCount = 1;
    .subresourceRange.aspectMask = 0;
};

VkImageCreateInfo phvk_default_image_create_info
{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    .pNext = nullptr;

    .flags = 0;

    .imageType = VK_IMAGE_TYPE_2D;

    .format = 0;
    .extent = 0;

    .mipLevels = 1;
    .arrayLayers = 1;

    .samples = VK_SAMPLE_COUNT_1_BIT;

    .tiling = VK_IMAGE_TILING_OPTIMAL;
    .usage = 0;

	.sharingMode = 0;
	.queueFamilyIndexCount = 0;
	.pQueueFamilyIndices = 0;
	.initialLayout = 0;
};

VkCommandPoolCreateInfo phvk_default_command_pool_create_info
{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    .pNext = nullptr;

    .flags = 0;
    .queueFamilyIndex = 0; 
};

VkCommandBufferAllocateInfo phvk_default_command_buffer_allocate_info
{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    .pNext = nullptr;

    .commandPool = 0;
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    .commandBufferCount = 0;
};

VkPipelineShaderStageCreateInfo phvk_default_pipeline_shader_stage_create_info
{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	.pNext = nullptr;

	.flags = 0;
	.stage = VK_SHADER_STAGE_ALL;
	.module = 0;
	.pName = nullptr;
	.pSpecializationInfo = nullptr;
};