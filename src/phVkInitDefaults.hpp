// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Default Vulkan Initializer Structures

#pragma once

#include <vulkan/vulkan.h>

VkImageViewCreateInfo phVkDefaultImageViewCreateInfo()
{
    VkImageViewCreateInfo temp {};

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
	VkImageCreateInfo temp {};

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
	VkCommandPoolCreateInfo temp {};

	temp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	temp.pNext = nullptr;
	//temp.flags = 0;
	temp.queueFamilyIndex = 0;

	return temp;
}

VkCommandBufferAllocateInfo phVkDefaultCommandBufferAllocateInfo()
{
	VkCommandBufferAllocateInfo temp {};

	temp.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	temp.pNext = nullptr;
	//temp.commandPool = 0;
	temp.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	temp.commandBufferCount = 0;

	return temp;
}

VkPipelineShaderStageCreateInfo phVkDefaultPipelineShader()
{
	VkPipelineShaderStageCreateInfo temp {};

	temp.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	temp.pNext = nullptr;
	//temp.flags = 0;
	temp.stage = VK_SHADER_STAGE_ALL;
	temp.module = 0;
	temp.pName = nullptr;
	temp.pSpecializationInfo = nullptr;
	
	return temp;
}

VkPipelineShaderStageCreateInfo phVkDefaultShaderStageCreateInfo()
{
	VkPipelineShaderStageCreateInfo temp {};

    temp.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    temp.pNext = nullptr;
    //temp.flags;
    //temp.stage;
    //temp.module;
    //temp.pName;
    //temp.pSpecializationInfo;

    return temp;
}

VkCommandBufferBeginInfo phVkDefaultCommandBufferBeginInfo()
{
    VkCommandBufferBeginInfo temp {};

    temp.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    temp.pNext = nullptr;
    //temp.flags;
    temp.pInheritanceInfo = nullptr;

    return temp;
}

VkCommandBufferSubmitInfo phVkDefaultCommandBufferSubmitInfo()
{
    VkCommandBufferSubmitInfo temp {};

    temp.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    temp.pNext = nullptr;
    //temp.commandBuffer;
    temp.deviceMask = 0;

    return temp;
}

VkSemaphoreSubmitInfo phVkDefaultSemaphoreSubmitInfo()
{
    VkSemaphoreSubmitInfo temp {};

    temp.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    temp.pNext = nullptr;
    //temp.semaphore;
    temp.value = 1;
    //temp.stageMask;
    temp.deviceIndex = 0;

    return temp;
}

VkSubmitInfo2 phVkDefaultSubmitInfo2()
{
    VkSubmitInfo2 temp {};

    temp.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    temp.pNext = nullptr;
    //temp.flags;

    temp.waitSemaphoreInfoCount = 0;
    temp.pWaitSemaphoreInfos = nullptr;

    temp.commandBufferInfoCount = 1;    // Assume there will be command buffer info at minimum
    //temp.pCommandBufferInfos;

    temp.signalSemaphoreInfoCount = 0;
    temp.pSignalSemaphoreInfos = nullptr;

    return temp;
}

VkPresentInfoKHR phVkDefaultPresentInfo()
{
    VkPresentInfoKHR temp {};

    temp.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    temp.pNext = nullptr;
    temp.waitSemaphoreCount = 0;
    temp.pWaitSemaphores = nullptr;
    temp.swapchainCount = 0;
    temp.pSwapchains = nullptr;
    temp.pImageIndices = nullptr;
    //temp.pResults = nullptr;

    return temp;
}

VkImageSubresourceRange phVkDefaultImageSubresourceRange()
{
    VkImageSubresourceRange temp {};

    temp.aspectMask = 0;
    temp.baseMipLevel = 0;
    temp.levelCount = VK_REMAINING_MIP_LEVELS;
    temp.baseArrayLayer = 0;
    temp.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return temp;
}


VkRenderingAttachmentInfo phVkDefaultColorAttachmentInfo()
{
    VkRenderingAttachmentInfo temp {};

    temp.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    temp.pNext = nullptr;

    //temp.imageView;
    temp.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    //temp.resolveMode;
    //temp.resolveImageView;
    //temp.resolveImageLayout;

    temp.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    temp.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //temp.clearValue;

    return temp;
}


// Note: Using reversed depth (near/far) where 1 is near and 0 is far
//	     This is a common optimization in Vulkan to avoid depth precision issues
VkRenderingAttachmentInfo phVkDefaultDepthAttachmentInfo()
{
    VkRenderingAttachmentInfo temp {};

    temp.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    temp.pNext = nullptr;

    //temp.imageView;
    temp.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    //temp.resolveMode;
    //temp.resolveImageView;
    //temp.resolveImageLayout; 

    temp.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    temp.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    temp.clearValue.depthStencil.depth = 0.f;

    return temp;
}



VkRenderingInfo phVkDefaultRenderingInfo()
{
    VkRenderingInfo temp {};

    temp.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    temp.pNext = nullptr;

    //temp.flags = 0;

    //temp.renderArea;
    temp.layerCount = 1;
    //temp.viewMask;
    temp.colorAttachmentCount = 1;
    temp.pColorAttachments = nullptr;
    temp.pDepthAttachment = nullptr;
    temp.pStencilAttachment = nullptr;

    return temp;
}

