// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Main Engine Class

#pragma once

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h> 

#define VMA_IMPLEMENTATION  // Must include in exactly ONE cpp file
#include <vk_mem_alloc.h>   // May be included elsewhere w/o VMA_IMPLEMENTATION

#include "Vec.hpp"
#include "Quat.hpp"
#include "Mat.hpp"
#include "array_list.hpp"

#include "phVkTypes.h"
#include "phVkInitDefaults.h"
#include "phVkPipelines.hpp"


// TODO: singleton?
template <typename T>
class phVkEngine
{
private:
    // PRIVATE DATA

    // Meta Data
    bool is_initialized = false;
    int frame_number = 0;
    bool stop_rendering = false;
    bool resize_requested = false;

    // Window
    SDL_Window* window = nullptr;

    // Vulkan
	VkInstance instance;                // Vulkan instance
	VkDebugUtilsMessengerEXT debug_messenger; // Debug output handle
    VkPhysicalDevice physical_device;	// GPU chosen as the default device
    VkDevice device;					// Vulkan device for commands
    VkSurfaceKHR surface;				// Vulkan window surface
    VkQueue graphics_queue;				// Graphics queue handle
    uint32_t graphics_queue_family;		// Graphics queue family
	VmaAllocator allocator;			    // Vulkan Memory Allocator (VMA)

    // Image/Swapchain Objects
    VkSwapchainKHR swapchain;			// Swapchain handle
    VkFormat swapchain_img_format;		// Image format	
    ArrayList<VkImage> swapchain_images;	// Image handles
    ArrayList<VkImageView> swapchain_image_views;	// Image view handles
    phVkImage draw_image;               // Swapchain draw image
	phVkImage depth_image; 	            // Swapchain depth image 

    // Frame Data (per-frame sync, command, and descriptor objects)
    phVkFrameData frames[FRAME_BUFFER_COUNT];	// Use getCurrentFrame() to access

    // Immediate Commands
	VkFence imm_fence;                  // Fence for immediate commands
    VkCommandBuffer imm_command_buffer; // Immediate command buffer
	VkCommandPool imm_command_pool;     // Immediate command pool

    // Descriptors
    phVkDescriptorAllocator global_descriptor_allocator;
    VkDescriptorSet draw_image_descriptors;
    VkDescriptorSetLayout gpu_scene_data_descriptor_layout;
    VkDescriptorSetLayout draw_image_descriptor_layout;
    VkDescriptorSetLayout single_image_descriptor_layout;

    // Pipelines
    phVkPipeline mesh_pipeline;


public:
    // PUBLIC FUNCTIONS

    // -- Init Functions --
    bool init(uint32_t width, uint32_t height, std::string title);
    bool initGUI();

    // -- Get Functions --
    phVkFrameData& getCurrentFrame() { return frames[frame_number % FRAME_BUFFER_COUNT]; };

    // -- Cleanup --
    void cleanup();

    phVkEngine() { ; }
    ~phVkEngine() { cleanup(); }

private:
    // PRIVATE FUNCTIONS

    // -- Init Functions --
    bool createWindow(uint32_t width, uint32_t height, std::string title);
    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initSyncObjects();
    void initDescriptors();
    void initPipelines();

    // -- Swapchain Functions --
    void createSwapchain(uint32_t width, uint32_t height);
    void destroySwapchain();
	void resizeSwapchain();

	// -- Pipeline Functions --
    void createBackgroundPipelines();
    void createMeshPipelines();
    void createMaterialPipelines();
};


// TODO: add configurability
template <typename T>
bool phVkEngine<T>::init(uint32_t width, uint32_t height, std::string title)
{
	if (!createWindow(width, height, title))
		return false;
    initVulkan();
	initSwapchain();
    initCommands();
    initSyncObjects();
    initDescriptors();
    initPipelines();

    return true;
}


template <typename T>
void phVkEngine<T>::cleanup()
{
	// -- Destroy resources in reverse order of creation --

    

    // Destroy descriptor set objects
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        frames[i].frame_descriptors.destroyPools(device);
    }
    global_descriptor_allocator.destroyPools(device);
    vkDestroyDescriptorSetLayout(device, draw_image_descriptor_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, single_image_descriptor_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, gpu_scene_data_descriptor_layout, nullptr);

    // Destroy sync objects
    vkDestroyFence(device, imm_fence, nullptr);
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        vkDestroyFence(device, frames[i].render_fence, nullptr);
        vkDestroySemaphore(device, frames[i].render_semaphore, nullptr);
        vkDestroySemaphore(device, frames[i].swapchain_semaphore, nullptr);
    }

    // Destroy command pools
    vkDestroyCommandPool(device, imm_command_pool, nullptr);
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) 
    {
        // Note: destroying the command pool also destroys buffers allocated from it
        vkDestroyCommandPool(device, frames[i].command_pool, nullptr);
    }

    // Destroy swapchain images
    vkDestroyImageView(device, depth_image.view, nullptr);
    vmaDestroyImage(allocator, depth_image.image, depth_image.allocation);
    vkDestroyImageView(device, draw_image.view, nullptr);
    vmaDestroyImage(allocator, draw_image.image, draw_image.allocation);

    // Destroy Vulkan configs (de-init Vulkan)
    vmaDestroyAllocator(allocator);
    vkDestroySurfaceKHR(instance, surface, nullptr);    // TODO - out of order?
    vkDestroyDevice(device, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debug_messenger);
    vkDestroyInstance(instance, nullptr);

    // Destroy the window
	SDL_DestroyWindow(window);
	
    // Quit SDL
	SDL_Quit();
}


template <typename T>
bool phVkEngine<T>::createWindow(uint32_t width, uint32_t height, std::string title)
{
    // Initialize SDL and create a window with it
    SDL_Init(SDL_INIT_VIDEO);

    // SDL window flags
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    // Create the window
    window = SDL_CreateWindow(
        title.c_str(),
        width,
        height,
        window_flags);

    if (window == nullptr)
    {
        return false;
    }
    else
    {
        return true;
    }
}


template <typename T>
void phVkEngine<T>::initVulkan()
{
    // -- Build Vulkan Instance --
    //
    vkb::InstanceBuilder builder;

    auto inst_ret = builder.set_app_name("Acid Engine Vulkan")
        .request_validation_layers(use_validation_layers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // Save handles to engine class
    instance = vkb_inst.instance;   
    debug_messenger = vkb_inst.debug_messenger;


    // -- Init Device --
    // 
    // Surface
    SDL_Vulkan_CreateSurface(window, instance, &surface);

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // Use vkbootstrap to select a GPU (physical device)
    // We want a GPU that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice vkb_physical_device = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(surface)
        .select()
        .value();

    // Use vkbootstrap to create the logical Vulkan device
    vkb::DeviceBuilder device_builder{ vkb_physical_device };
    vkb::Device vkbdevice = device_builder.build().value();

    // Save handles to engine class
    device = vkbdevice.device;
    physical_device = vkb_physical_device.physical_device;


    // -- Init Vulkan Work Queue --
    // 
    // Use vkbootstrap to get a graphics queue
    graphics_queue = vkbdevice.get_queue(vkb::QueueType::graphics).value();
    graphics_queue_family = vkbdevice.get_queue_index(vkb::QueueType::graphics).value();


    // -- Init Vulkan Memory Allocator (VMA) --
    //
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.instance = instance;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_info, &allocator);

    return true;
}


template <typename T>
void phVkEngine<T>::initSwapchain()
{
    // -- Create Swapchain --
    //
    int window_size[2]{ 0 };
	SDL_GetWindowSize(window, &window_size[0], &window_size[1]);
    createSwapchain(window_size[0], window_size[1]);

    // -- Init Draw Image --
    //
    // Set the draw image to monitor dimensions
    // Will only render to a portion that matches the window size
    VkExtent3D draw_extent =
    {
        // TODO: Still crashes if resizing above these dimensions
        // TODO: Pull monitor dimensions rather than hard-coding
        2560,   // Width
		1440,   // Height
        1       // Depth value
    };

    // Draw format hardcoded
    // 64 bits per pixel
    // May be overkill, but useful in some cases
    draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    draw_image.extent = draw_extent;

    // Define image usages
    VkImageUsageFlags draw_image_usages{};
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo img_info = vkinit::image_create_info(draw_image.format, 
        draw_image_usages, draw_extent);

    // Allocate draw image from GPU local memory
    // Configure for GPU-only access and fastest memory
    VmaAllocationCreateInfo img_alloc_info = {};
    img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    img_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the image
    vmaCreateImage(allocator, &img_info, &img_alloc_info, &draw_image.image, 
        &draw_image.allocation, nullptr);

    // Build an image-view for the draw image to use for rendering
    // vkguide.dev always pairs vkimages with "default" imageview
    VkImageViewCreateInfo view_info = phvk_default_image_view_create_info;
	view_info.format = draw_image.format;
	view_info.image = draw_image.image;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &draw_image.view));




    // -- Init Depth Image --
    //
    depth_image.format = VK_FORMAT_D32_SFLOAT;
    depth_image.extent = draw_extent;
    VkImageUsageFlags depth_image_usages{};
    depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;  // Key to depth pass

    VkImageCreateInfo dimg_info = phvk_default_image_create_info;
	dimg_info.format = depth_image.format;
	dimg_info.extent = draw_extent;
	dimg_info.usage = depth_image_usages;

    // Allocate and create the image
    vmaCreateImage(allocator, &dimg_info, &img_alloc_info, &depth_image.image, 
        &depth_image.allocation, nullptr);

    // Build an image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info = phvk_default_image_view_create_info;
    dview_info.format = depth_image.format;
    dview_info.image = depth_image.image;
    dview_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VK_CHECK(vkCreateImageView(device, &dview_info, nullptr, &depth_image.view));
}


template <typename T>
void phVkEngine<T>::initCommands()
{
    // -- Command Pool --
    //
    // Create a command pool for commands submitted to the graphics queue
    // Configure to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo command_pool_info = phvk_default_command_pool_create_info;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_info.queueFamilyIndex = graphics_queue_family;

    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        VK_CHECK(vkCreateCommandPool(device, &command_pool_info, nullptr, &frames[i].command_pool));

        // Allocate the default command buffer for rendering
        VkCommandBufferAllocateInfo cmd_alloc_info = phvk_default_command_buffer_allocate_info;
        cmd_alloc_info.commandPool = frames[i].command_pool;
        cmd_alloc_info.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &frames[i].command_buffer));
    }


    // -- Immediate Commands --
    //
    VK_CHECK(vkCreateCommandPool(device, &command_pool_info, nullptr, &imm_command_pool));

    // Allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmd_alloc_info = phvk_default_command_buffer_allocate_info;
	cmd_alloc_info.commandPool = imm_command_pool;
	cmd_alloc_info.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &imm_command_buffer));

}


template<typename T>
void phVkEngine<T>::initSyncObjects()
{
    // Create CPU/GPU sync objects
    // One fence to signal when the gpu has finished rendering the frame
    // Two semaphores to synchronize rendering with swapchain

    VkFenceCreateInfo fence_create_info = {}
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled to avoid blocking

    VkSemaphoreCreateInfo semaphore_create_info = {}
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0;

    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) 
    {
        VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &frames[i].render_fence));

        VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &frames[i].swapchain_semaphore));
        VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &frames[i].render_semaphore));
    }

    // Immediate commands fence
    VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &imm_fence));
}


template<typename T>
void phVkEngine<T>::initDescriptors()
{
    // Descriptor pool type counts
    std::vector<phVkDescriptorAllocator::PoolSizeRatio> sizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 } };

    // Allocate 10 sets
    global_descriptor_allocator.init(device, 10, sizes);

    // Descriptor set layout - compute draw image
    // Note: using block/naked scope to avoid clearing the builder
    {
        phVkDescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        draw_image_descriptor_layout = builder.build(device,
            VK_SHADER_STAGE_COMPUTE_BIT);
    }
	// Descriptor set layout - GPU scene data
    {
        phVkDescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        gpu_scene_data_descriptor_layout = builder.build(device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
	// Descriptor set layout - single image
    {
        phVkDescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        single_image_descriptor_layout = builder.build(device,
            VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // Allocate descriptor set - compute draw image
    draw_image_descriptors = global_descriptor_allocator.allocate(device, draw_image_descriptor_layout);

    // Update descriptor set info - compute draw image
    phVkDescriptorWriter writer;
    writer.writeImage(0, draw_image.view, VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.updateSet(device, draw_image_descriptors);

    // Initialize per-frame descriptor pools
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        std::vector<phVkDescriptorAllocator::PoolSizeRatio> frame_sizes =
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        frames[i].frame_descriptors = phVkDescriptorAllocator{};
		// 1000 sets per pool (TODO: make configurable)
        frames[i].frame_descriptors.init(device, 1000, frame_sizes);
    }
}


template<typename T>
void phVkEngine<T>::initPipelines()
{
    createBackgroundPipeline();

    createMeshPipeline();

    createMaterialPipeline();
}


template <typename T>
void phVkEngine<T>::createSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchain_builder{ physical_device, device, surface };

    swapchain_img_format = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkb_swapchain = swapchain_builder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = swapchain_img_format, 
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        
        .set_desired_present_mode(SWAPCHAIN_MODE)   // See constexpr
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    swapchain_extent = vkb_swapchain.extent;
    swapchain = vkb_swapchain.swapchain;
    for (unsigned int i = 0; i < vkb_swapchain.get_images().value().size; i++)
    {
        // TODO: Inefficient conversion from vector to ArrayList
		//       Eliminate vkb or make an ArrayList(vector) constructor?
        swapchain_images.push_back(vkb_swapchain.get_images().value()[i]);
    }
    for (unsigned int i = 0; i < vkb_swapchain.get_image_views().value().size; i++)
    {
        // TODO: Inefficient conversion from vector to ArrayList
        //       Eliminate vkb or make an ArrayList(vector) constructor?
        swapchain_images.push_back(vkb_swapchain.get_image_views().value()[i]);
    }
}


template <typename T>
void phVkEngine<T>::destroySwapchain()
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    // Destroy swapchain resources
    for (int i = 0; i < swapchain_image_views.size(); i++)
    {
        vkDestroyImageView(device, swapchain_image_views[i], nullptr);
    }
}


template <typename T>
void phVkEngine<T>::resizeSwapchain()
{
    vkDeviceWaitIdle(device);

    destroySwapchain();

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    createSwapchain(w, h);
}


template <typename T>
void phVkEngine<T>::createBackgroundPipelines()
{
    if (mesh_pipeline.device == 0)
        mesh_pipeline = phVkPipeline(device, VULKAN_GRAPHICS_PIPELINE, viewport, scissor);

    // Shader modules
    mesh_pipeline.loadVertexShader("../../../../shaders/colored_triangle_mesh.vert.spv");   // TODO: change
    mesh_pipeline.loadFragmentShader("../../../../shaders/colored_triangle_mesh.frag.spv"); // TODO: change

}


template <typename T>
void phVkEngine<T>::createMeshPipelines()
{
    if (mesh_pipeline.device == 0)
        mesh_pipeline = phVkPipeline(device, VULKAN_COMPUTE_PIPELINE, viewport, scissor);

    // Shader modules
    mesh_pipeline.loadComputeShader("../../../../shaders/sky.comp.spv");   // TODO: change

    // Build the pipeline
    mesh_pipeline.createPipeline();

    // Shader modules have been compiled into the pipeline and are no longer needed
    mesh_pipeline.destroyShaderModules();
}


template <typename T>
void phVkEngine<T>::createMaterialPipelines()
{

}

