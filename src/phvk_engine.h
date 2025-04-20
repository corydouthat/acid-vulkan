// Copyright (c) 2025, Cory Douthat
// Based on vkguide.net - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Engine class

#pragma once

#include "phvk_types.h"

#include <vector>

#include <vk_mem_alloc.h>

#include <deque>
#include <functional>

#include "phvk_descriptors.h"

// Number of buffering frames
constexpr unsigned int FRAME_OVERLAP = 2;

// Queue for deleting objects in FIFO order
struct DeleteQueue
{
	std::deque<std::function<void()>> fifo;

	void push_function(std::function<void()>&& function) 
	{
		fifo.push_back(function);
	}

	void flush() 
	{
		// Reverse iterate the deletion queue to execute all the functions
		for (auto it = fifo.rbegin(); it != fifo.rend(); it++) 
		{
			(*it)(); // Call function
		}

		fifo.clear();
	}
};


//	Frame data for queue
struct FrameData 
{
	VkSemaphore swapchain_semaphore, render_semaphore;
	VkFence render_fence;

	VkCommandPool command_pool;
	VkCommandBuffer main_command_buffer;

	DeleteQueue delete_queue;
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

	// Vulkan image resources for drawing
	AllocatedImage draw_image;
	VkExtent2D draw_extent;

	// Queue / frame objects
	FrameData frames[FRAME_OVERLAP];	// Use get_current_frame() to access
	VkQueue graphics_queue;				// Graphics queue handle
	uint32_t graphics_queue_family;		// Graphics queue family

	// Descriptor sets
	DescriptorAllocator global_descriptor_allocator;
	VkDescriptorSet draw_image_descriptors;
	VkDescriptorSetLayout draw_image_descriptor_layout;

	// Pipelines
	VkPipeline gradient_pipeline;
	VkPipelineLayout gradient_pipeline_layout;

	// Immediate-submit structures
	VkFence imm_fence;
	VkCommandBuffer imm_command_buffer;
	VkCommandPool imm_command_pool;

	// Deletion Queue
	DeleteQueue main_delete_queue;

	// Vulkan Memory Allocator (VMA)
	VmaAllocator allocator;

	// *** Get Functions ***

	FrameData& getCurrentFrame() { return frames[frame_number % FRAME_OVERLAP]; };
	static phVkEngine& getLoadedEngine();	// Singleton implementation


	// *** Engine Functions ***

	// Initializes engine
	void init();

	// Shut down and cleanup
	void cleanup();

	// Draw loop
	void draw();
	void drawBackground(VkCommandBuffer cmd);
	void drawImgui(VkCommandBuffer cmd, VkImageView target_image_view);

	// Immediate submit
	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

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
	void initDescriptors();
	void initPipelines();
	void initBackgroundPipelines();
	void initImgui();
};