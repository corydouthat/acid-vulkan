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
#include "phvk_loader.h"

// Number of buffering frames
constexpr unsigned int FRAME_OVERLAP = 2;

// DEBUG: Validation layers switch
constexpr bool use_validation_layers = true;

// Queue for deleting objects in FIFO order
struct DeleteQueue
{
	std::deque<std::function<void()>> fifo;

	void pushFunction(std::function<void()>&& function) 
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

	DescriptorAllocatorGrowable frame_descriptors;

	DeleteQueue delete_queue;
};

struct RenderObject
{
	uint32_t index_count;
	uint32_t first_index;
	VkBuffer index_buffer;

	MaterialInstance* material;

	Mat4f transform;
	VkDeviceAddress vertex_buffer_address;
};

struct DrawContext 
{
	std::vector<RenderObject> opaque_surfaces;
	std::vector<RenderObject> transparent_surfaces;
};

struct MeshNode : public Node 
{

	std::shared_ptr<MeshAsset> mesh;

	virtual void draw(const Mat4f& top_matrix, DrawContext& ctx) override;
};

struct ComputePushConstants 
{
	Vec4f data1;
	Vec4f data2;
	Vec4f data3;
	Vec4f data4;
};

struct ComputeEffect 
{
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

struct GLTFMetallicRoughness 
{
	MaterialPipeline opaque_pipeline;
	MaterialPipeline transparent_pipeline;

	VkDescriptorSetLayout material_layout;

	struct MaterialConstants 
	{
		Vec4f color_factors;
		Vec4f metal_rough_factors;
		Vec4f extra[14];	// Pad to 256 bytes for uniform buffers
	};

	struct MaterialResources 
	{
		AllocatedImage color_image;
		VkSampler color_sampler;
		AllocatedImage metal_rough_image;
		VkSampler metal_rough_sampler;
		VkBuffer data_buffer;
		uint32_t data_buffer_offset;
	};

	DescriptorWriter writer;

	void buildPipelines(phVkEngine* engine);
	void clearResources(VkDevice device);

	MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, 
		const MaterialResources& resources, DescriptorAllocatorGrowable& descriptor_allocator);
};


class phVkEngine 
{
public:

	// *** Member Data ***

	// TODO: can some/all data be made private and add accessor functions?
	bool is_initialized { false };
	int frame_number { 0 };
	bool stop_rendering { false };
	bool resize_requested { false };

	struct SDL_Window* window { nullptr };

	// Extents
	VkExtent2D window_extent{ 1700 , 900 };
	VkExtent2D draw_extent;
	VkExtent2D swapchain_extent;
	float render_scale = { 1.f };

	// Background effects objects
	std::vector<ComputeEffect> background_effects;
	int current_background_effect { 0 };

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

	// GPU Scene Data
	GPUSceneData scene_data;

	// Image resources
	AllocatedImage draw_image;
	AllocatedImage depth_image;

	AllocatedImage white_image;		// Default texture image
	AllocatedImage black_image;		// Default texture image
	AllocatedImage grey_image;		// Default texture image
	AllocatedImage error_checkerboard_image;	// Default texture image

	// Samplers
	VkSampler default_sampler_linear;
	VkSampler default_sampler_nearest;

	// Material
	MaterialInstance default_data;	// TODO: re-name?
	GLTFMetallicRoughness metal_rough_material;

	// Queue / frame objects
	FrameData frames[FRAME_OVERLAP];	// Use get_current_frame() to access
	VkQueue graphics_queue;				// Graphics queue handle
	uint32_t graphics_queue_family;		// Graphics queue family

	// Immediate-submit structures
	VkFence imm_fence;
	VkCommandBuffer imm_command_buffer;
	VkCommandPool imm_command_pool;

	// Descriptor sets
	DescriptorAllocatorGrowable global_descriptor_allocator;
	VkDescriptorSet draw_image_descriptors;
	VkDescriptorSetLayout gpu_scene_data_descriptor_layout;
	VkDescriptorSetLayout draw_image_descriptor_layout;
	VkDescriptorSetLayout single_image_descriptor_layout;

	// Pipelines
	VkPipeline gradient_pipeline;
	VkPipelineLayout gradient_pipeline_layout;
	VkPipeline mesh_pipeline;
	VkPipelineLayout mesh_pipeline_layout;

	// Mesh data
	std::vector<std::shared_ptr<MeshAsset>> test_meshes;

	// Vulkan Memory Allocator (VMA)
	VmaAllocator allocator;

	// Deletion Queue
	DeleteQueue main_delete_queue;



	// *** Get Functions ***

	FrameData& getCurrentFrame() { return frames[frame_number % FRAME_OVERLAP]; };
	static phVkEngine& getLoadedEngine();	// Singleton implementation


	// *** Engine Functions ***

	// Initializes engine
	void init();

	// Execute main loop (including draw)
	void run();

	// Shut down and cleanup
	void cleanup();

	// Draw loop
	void draw();
	void drawBackground(VkCommandBuffer cmd);
	void drawGeometry(VkCommandBuffer cmd);
	void drawImgui(VkCommandBuffer cmd, VkImageView target_image_view);

	// Immediate submit
	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	// Upload a mesh to the GPU
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	// Images
	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroyImage(const AllocatedImage& img);

	// Buffers
	AllocatedBuffer createBuffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
	void destroyBuffer(const AllocatedBuffer& buffer);

private:

	// Swapchain functions
	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain();
	void resizeSwapchain();

	// Private initializers
	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructures();
	void initDescriptors();
	void initPipelines();
	void initBackgroundPipelines();
	void initMeshPipeline();
	void initImgui();
	void initDefaultData();
};