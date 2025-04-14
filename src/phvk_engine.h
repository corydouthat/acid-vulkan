// Copyright (c) 2025, Cory Douthat
//
// Acid Game Engine - Vulkan
// Engine class

#pragma once

#include "phvk_types.h"

// Number of buffering frames
constexpr unsigned int FRAME_OVERLAP = 2;

//	Frame data for queue
struct FrameData 
{
	VkSemaphore swapchain_semaphore, render_semaphore;
	VkFence render_fence;

	VkCommandPool command_pool;
	VkCommandBuffer main_command_buffer;
};


class phVkEngine {
public:

	// *** Member Data ***

	// TODO: can some/all data be made private and add accessor functions?
	bool is_initialized { false };
	int frame_number { 0 };
	bool stop_rendering { false };
	VkExtent2D window_extent { 1700 , 900 };

	struct SDL_Window* window { nullptr };

	// Instance objects
	VkInstance instance;				// Vulkan library handle
	VkDebugUtilsMessengerEXT debug_messenger;	// Vulkan debug output handle
	VkPhysicalDevice physical_device;	// GPU chosen as the default device
	VkDevice device;					// Vulkan device for commands
	VkSurfaceKHR surface;				// Vulkan window surface

	// Swapchain objects
	VkSwapchainKHR swapchain;			// Swapchain handle
	VkFormat swapchain_img_format;		// Image format	
	std::vector<VkImage> swapchain_images;	// Image handles
	std::vector<VkImageView> swapchain_image_views;	// Image view handles
	VkExtent2D swapchain_extent;			// Swapchain extent

	// Queue / frame objects
	FrameData frames[FRAME_OVERLAP];	// Use get_current_frame() to access
	VkQueue graphics_queue;				// Graphics queue handle
	uint32_t graphics_queue_family;		// Graphics queue family



	// *** Get Functions ***

	FrameData& getCurrentFrame() { return frames[frame_number % FRAME_OVERLAP]; };



	// *** Engine Functions ***

	// Gets the loaded engine instance
	static phVkEngine& getLoadedEngine();

	// Initializes engine
	void init();

	// Shut down and cleanup
	void cleanup();

	// Draw loop
	void draw();

	// Execute main loop (including draw)
	void run();

private:

	// Create swapchain
	void createSwapchain(uint32_t width, uint32_t height);
	// Destroy swapchain
	void destroySwapchain();

	// Private initializers
	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructures();

};