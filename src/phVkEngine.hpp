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
#include "phVkDescriptors.hpp"
#include "phVkPipelines.hpp"
#include "phVkImages.hpp"

#include "phVkScene.hpp"

#include "phVkCamera.hpp"


// TODO: singleton?
template <typename T>
class phVkEngine
{
private:
    // PRIVATE DATA

    // Meta Data
    bool is_initialized = false;
    bool stop_rendering = false;
    bool resize_requested = false;
    bool sdl_quit = false;
    int frame_number = 0;

    // SDL / Window
    SDL_Window* window = nullptr;

    // Scene
    ArrayList<phVkScene<T>> scenes;
    GPUSceneData scene_data;

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

    // Push Constants
    Vec4<T>[4] compute_push_constants;

    // Buffer
    AllocatedBuffer gpu_scene_data_buffer;

    // Pipelines
	phVkPipeline background_pipeline;
    phVkPipeline mesh_pipeline;


public:
    // PUBLIC DATA
    ArrayList<phVkCamera<T>> cameras;
    unsigned int active_camera = 0;


    // PUBLIC FUNCTIONS

    phVkEngine() { ; }
    ~phVkEngine() { cleanup(); }

    // -- Init Functions --
    bool init(uint32_t width, uint32_t height, std::string title);
    bool initGUI();
    bool loadScene(std::string file_path);

    // -- Get Functions --
    phVkFrameData& getCurrentFrame() { return frames[frame_number % FRAME_BUFFER_COUNT]; };

    // -- Run Functions --
    void run();

    // -- Buffer Functions --
    AllocatedBuffer createBuffer(size_t alloc_size,
        VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
    void destroyBuffer(const AllocatedBuffer& buffer);

    // -- Cleanup --
    void cleanup();

private:
    // PRIVATE FUNCTIONS

    // -- Run Functions --
    void draw();
    void renderImGui();
    void drawBackground();
    void drawMesh();

    // -- Init Functions --
    bool createWindow(uint32_t width, uint32_t height, std::string title);
    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initSyncObjects();
    void initDescriptors();
    void initPipelines();
    void initBuffers();

    // -- Swapchain Functions --
    void createSwapchain(uint32_t width, uint32_t height);
    void destroySwapchain();
	void resizeSwapchain();
    VkExtent2D determineDrawExtent();

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
    initBuffers();

    return true;
}


template <typename T>
bool phVkEngine<T>::loadScene(std::string file_path)
{
    int scene = scenes.push(phVkScene<T>());

    scenes[scene].load(file_path);

    scenes[scene].initVulkan();
}


template <typename T>
void phVkEngine<T>::run()
{
    SDL_Event sdl_event;

    // -- Handle SDL Events --
    while (SDL_PollEvent(&sdl_event) != 0)  // Until queue is empty
    {
        // Quit
        if (sdl_event.type == SDL_QUIT)
            sdl_quit = true;

        // Window minimize/restore
        if (sdl_event.type == SDL_WINDOWEVENT)
        {
            if (sdl_event.window.event == SDL_WINDOWEVENT_MINIMIZED)
                stop_rendering = true;
            if (sdl_event.window.event == SDL_WINDOWEVENT_RESTORED)
                stop_rendering = false;
        }

        // Key/mouse callbacks
        // TODO

        // Send SDL event to imgui for handling
        ImGui_ImplSDL3_ProcessEvent(&sdl_event);
    }


    // -- Window Resize --
    if (resize_requested)
    {
        resizeSwapchain();
        resize_requested = false;
    }


    // -- Draw --
    draw();

}


template <typename T>
AllocatedBuffer phVkEngine<T>::createBuffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage)
{
    VkBufferCreateInfo buffer_info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.pNext = nullptr;
    buffer_info.size = alloc_size;

    buffer_info.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memory_usage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer new_buffer;

    // Allocate the buffer
    VK_CHECK(vmaCreateBuffer(allocator, &buffer_info, &vmaallocInfo,
        &new_buffer.buffer, &new_buffer.allocation, &new_buffer.info));

    return new_buffer;
}


template <typename T>
void phVkEngine<T>::destroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}


template <typename T>
void phVkEngine<T>::cleanup()
{
	// -- Destroy resources in reverse order of creation --

    // Scene
    scenes.clear(); // Rely on destructors to detroy buffers

    // Buffers
    destroyBuffer(scene_data_buffer);

    // Pipelines
	mesh_pipeline.clearToDefaults();
	background_pipeline.clearToDefaults();
    // TODO: material pipelines

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
void phVkEngine<T>::draw()
{
    // -- Render GUI --
    // Does not draw to a Vulkan impage yet
    renderImGui();


    // -- Last Frame Wrap-up --
    // Wait for previous frame to finish
    VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame().render_fence, VK_TRUE, 1000000000));

    // Destroy frame's old uniform buffer(s) and descriptor set(s)
    // TODO: change this to only make the buffers once and not re-allocate every frame
    // Is this because there are separate descriptor sets allocated from the pool for each mesh object every frame?
    destroyBuffer(getCurrentFrame().scene_data_buffer);
    getCurrentFrame().frame_descriptors.clearPools(device);

    // TODO: or can we re-allocate the buffers / pools here (currently in drawGeometry)
    // Also descriptor set?

    // Acquire an image from the swap chain
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, 1000000000,
        getCurrentFrame().swapchain_semaphore,
        VK_NULL_HANDLE, &image_index);

    // Check if swapchain needs to be recreated
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        resize_requested = true;
        return;
    }
    else if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to acquire swap chain image!");

    
    // -- New Frame Setup -- 
    // 
    // Reset fence for current frame
    VK_CHECK(vkResetFences(device, 1, &getCurrentFrame().render_fence));

    // Reset command buffer
    VK_CHECK(vkResetCommandBuffer(getCurrentFrame().main_command_buffer, 0));

    // Set up command buffer
    VkCommandBuffer cmd = getCurrentFrame().main_command_buffer;
    VkCommandBufferBeginInfo cmd_begin_info = phVkDefaultCommandBufferBeginInfo();
    cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // Transition draw image to general layout for writing
    vkutil::transition_image(cmd, draw_image.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    // Transition depth image to depth attachment
    vkutil::transition_image(cmd, depth_image.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);



    // -- Set Viewport and Scissor
    //
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)window_extent.width;
    viewport.height = (float)window_extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = window_extent.width;
    scissor.extent.height = window_extent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);


    // -- Draw Background --
    drawBackground();


    // -- Transition Image --
    // Transition for graphics drawing layout
    vkutil::transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);


    // -- Draw Mesh --
    drawMesh();


    // -- Copy Draws to Swapchain --
    // Transition draw image to transfer layout
    vkutil::transition_image(cmd, draw_image.image,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // Transition swapchain image to transfer layout
    vkutil::transition_image(cmd, swapchain_images[swapchain_image_index],
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Determine actual draw extents
    VkExtent2D draw_extent = determineDrawExtent();

    // Copy draw image into swapchain
    vkutil::copy_image_to_image(cmd, draw_image.image,
        swapchain_images[swapchain_image_index], draw_extent, swapchain_extent);


    // -- GUI Draw --
    // Transition swapchain image to appropriate layout for ImGui
    vkutil::transition_image(cmd, swapchain_images[swapchain_image_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Draw ImGui
    drawImgui(cmd, swapchain_image_views[swapchain_image_index]);


    // -- Present Layout --
    // Transition swapchain image to present layout
    vkutil::transition_image(cmd, swapchain_images[swapchain_image_index],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);


    // -- End Command Buffer --
    VK_CHECK(vkEndCommandBuffer(cmd));


    // -- Submit Command Buffer --
    VkCommandBufferSubmitInfo cmd_info = phVkDefaultCommandBufferSubmitInfo;
    cmd_info.commandBuffer = cmd;

    VkSemaphoreSubmitInfo wait_info = phVkDefaultSemaphoreSubmitInfo();
    wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    wait_info.semaphore = getCurrentFrame().swapchain_semaphore;
    VkSemaphoreSubmitInfo signal_info = phVkDefaultSemaphoreSubmitInfo();
    signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    signal_info.semaphore = getCurrentFrame().render_semaphore;

    VkSubmitInfo2 submit = phVkDefaultSubmitInfo2();
    submit.waitSemaphoreInfoCount = wait_info == nullptr ? 0 : 1;
    submit.pWaitSemaphoreInfos = wait_info;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = cmd_info;
    submit.signalSemaphoreInfoCount = signal_info == nullptr ? 0 : 1;
    submit.pSignalSemaphoreInfos = signal_info;

    // Submit command buffer to the queue
    // Assign render fence
    VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit, getCurrentFrame().render_fence));


    // -- Present Image --
    VkPresentInfoKHR present_info = phVkDefaultPresentInfo();

    present_info.pSwapchains = &swapchain;
    present_info.swapchainCount = 1;

    present_info.pWaitSemaphores = &getCurrentFrame().render_semaphore;
    present_info.waitSemaphoreCount = 1;

    present_info.pImageIndices = &swapchain_image_index;

    VkResult present_result = vkQueuePresentKHR(graphics_queue, &present_info);


    // -- Final Steps --
    
    // TODO: remove - this does nothing since the previous check has a 'return'?
    //if (result == VK_ERROR_OUT_OF_DATE_KHR)
    //{
    //    resize_requested = true;
    //    return;
    //}
    
    frame_number++;
}


template <typename T>
void phVkEngine<T>::renderImGui()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Hellow World"))
    {
        ImGui::Text("Frame Number: ", frame_number);

        ImGui::End();
    }

    // Internal - does not draw to a Vulkan image
    ImGui::Render();
}


template <typename T>
void phVkEngine<T>::drawBackground()
{
    // TODO: temporary
    compute_push_constants[0] = Vec4<T>(1, 0, 0, 0);
    compute_push_constants[1] = Vec4<T>(0, 1, 0, 0);
    compute_push_constants[2] = Vec4<T>(0, 0, 1, 0);
    compute_push_constants[3] = Vec4<T>(0, 0, 0, 1);

    // -- Bind Pipeline --
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, background_pipeline.pipeline);

    // -- Bind Descriptor Sets --
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, background_pipeline.layout,
        0, 1, &draw_image_descriptors, 0, nullptr);

    // -- Push Constants --
    vkCmdPushConstants(cmd, background_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(ComputePushConstants), &compute_push_constants);

    // -- Dispatch --
    // 16x16 workgroup
    // TODO: confirm draw_image.extent - vkguide.dev uses window_extent
    vkCmdDispatch(cmd, std::ceil(draw_image.extent.width / 16.0),
        std::ceil(draw_image.extent.height / 16.0), 1);
}


template <typename T>
void phVkEngine<T>::drawMesh()
{
    // -- Bind Pipeline --
    // TODO: add support for unique material pipelines
    // for now, using a single shared geometry pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline.pipeline);


    // -- Scene Data --
    // Allocate Descriptor set for scene data buffer
    // TODO: don't reallocate every time?
    VkDescriptorSet global_descriptor = getCurrentFrame().frame_descriptors.allocate(device,
        gpu_scene_data_descriptor_layout);

    // Write the local scene data to (CPU?) buffer
    // TODO: why not just reference the pointer directly instead of making a new one?
    GPUSceneData* scene_data_uniform = (GPUSceneData*)gpu_scene_data_buffer.allocation->GetMappedData();
    *scene_data_uniform = scene_data;

    // Write scene data to (GPU?) buffer
    // Update scene data descriptor
    DescriptorWriter writer;
    writer.writeBuffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);         // Queues up the buffer
    writer.updateSet(device, global_descriptor);    // Assigns the global_descriptor set pointer

    // Bind scene data global descriptor (slot 0)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline.layout, 0, 1,
        &global_descriptor, 0, nullptr);


    // -- Rendering Info / Image Attachments --
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(draw_image.view,
        nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(depth_image.view,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    VkRenderingInfo render_info = vkinit::rendering_info(window_extent,
        &colorAttachment, &depthAttachment);


    // -- Start Timer --
    auto start = std::chrono::system_clock::now();


    // -- Begin Rendering --
    vkCmdBeginRendering(cmd, &render_info);


    // -- Draw: Loop Objects --
    // TODO: add IsVisible check to avoid drawing objects that aren't in frame
    // TODO: sort by mesh and material for efficiency
    for (unsigned int s = 0; s < scenes.getCount(); s++)
    {
        for (unsigned int i = 0; i < scenes[s].models.getCount(); i++)
        {
            // Push constants - Model-specific
            GPUDrawPushConstants push_constants;
            push_constants.world_matrix = scenes[s].models.transform;

            for (unsigned int j = 0; j < scenes[s].models[i].sets.getCount(); j++)
            {
                unsigned int mesh_i = scenes[s].models[i].sets[j].mesh_i;

                // Push constants - Mesh-specific
                push_constants.vertex_buffer_address = scenes[s].meshes[mesh_i].vertex_buffer_address;
                // Push
                vkCmdPushConstants(cmd, mesh_pipeline.layout,
                    VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

                //// TODO TODO TODO
                //// Bind per-object material set descriptor (slot 1)
                //// TODO: need to actually create the materials
                //vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline.layout, 1, 1,
                //    &r.material->material_set, 0, nullptr);

                // Bind index
                vkCmdBindIndexBuffer(cmd, scenes[s].meshes[mesh_i].index_buffer.buffer, 
                    0, VK_INDEX_TYPE_UINT32);

                // Vulkan Draw
                vkCmdDrawIndexed(cmd, scenes[s].meshes[mesh_i].vertices.count, 1, 0, 0, 0);
            }
        }
    }


    // -- Stop Timer --
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.mesh_draw_time = elapsed.count() / 1000.f;    // TODO: adapt for my engine

    // -- End Rendering --
    vkCmdEndRendering(cmd);
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


template<typename T>
void phVkEngine<T>::initBuffers()
{
    AllocatedBuffer scene_data_buffer = createBuffer(sizeof(GPUSceneData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
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
VkExtent2D phVkEngine<T>::determineDrawExtent()
{
    // TODO: add checks for minImageExtent and maxImageExtent capabilities
    // TODO: add render scaling

    // TODO: move this to member data?
    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities));

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
    {
        return capabilities.currentExtent;
    }
    else 
    {
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);

        VkExtent2D actual_extent = 
        {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actual_extent.width = std::clamp(actual_extent.width, 
            capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, 
            capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }
}


template <typename T>
void phVkEngine<T>::createBackgroundPipelines()
{
    if (mesh_pipeline.device == 0)
        mesh_pipeline = phVkPipeline(device, VULKAN_GRAPHICS_PIPELINE, viewport, scissor);

    // Shader modules
    mesh_pipeline.loadVertexShader("../../../../shaders/colored_triangle_mesh.vert.spv");   // TODO: change
    mesh_pipeline.loadFragmentShader("../../../../shaders/colored_triangle_mesh.frag.spv"); // TODO: change



    // TODO!!


}


template <typename T>
void phVkEngine<T>::createMeshPipelines()
{
    if (mesh_pipeline.device == 0)
        mesh_pipeline = phVkPipeline(device, VULKAN_COMPUTE_PIPELINE, viewport, scissor);

    // Shader modules
    mesh_pipeline.loadComputeShader("../../../../shaders/sky.comp.spv");   // TODO: change



    // TODO!!



    // Build the pipeline
    mesh_pipeline.createPipeline();

    // Shader modules have been compiled into the pipeline and are no longer needed
    mesh_pipeline.destroyShaderModules();
}


template <typename T>
void phVkEngine<T>::createMaterialPipelines()
{
    // TODO
}

