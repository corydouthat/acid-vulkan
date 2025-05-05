// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Default Vulkan Initializer Structures

#include <vulkan/vulkan.h>

VkImageViewCreateInfo phvk_default_image_view_create_info
{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    .pNext = nullptr;

	//.flags = ;
    //.image = ;
    .viewType = VK_IMAGE_VIEW_TYPE_2D;
    //.format = ;
    //.compoonents = ;
    .subresourceRange.baseMipLevel = 0;
    .subresourceRange.levelCount = 1;
    .subresourceRange.baseArrayLayer = 0;
    .subresourceRange.layerCount = 1;
    //.subresourceRange.aspectMask = aspectFlags;
};

VkImageCreateInfo phvk_default_image_create_info
{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    .pNext = nullptr;

    //.flags = ;

    .imageType = VK_IMAGE_TYPE_2D;

    //.format = ;
    //.extent = ;

    .mipLevels = 1;
    .arrayLayers = 1;

	// Used for MSAA, but not needed for now
    .samples = VK_SAMPLE_COUNT_1_BIT;

    // Optimal tiling for GPU
    .tiling = VK_IMAGE_TILING_OPTIMAL;
    //.usage = ;

	//.sharingMode = ;
	//.queueFamilyIndexCount = ;
	//.pQueueFamilyIndices = ;
	//.initialLayout = ;
};

VkCommandPoolCreateInfo phvk_default_command_pool_create_info
{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    .pNext = nullptr;

    .flags = 0;                 // Initialize to 0 for safety
    .queueFamilyIndex = 0;	    // Initialize to 0 for safety   
};

VkCommandBufferAllocateInfo phvk_default_command_buffer_allocate_info
{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    .pNext = nullptr;

    .commandPool = 0;           // Initialize to 0 for safety
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    .commandBufferCount = 0;    // Initialize to 0 for safety
}
;