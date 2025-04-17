// Copyright (c) 2025, Cory Douthat
// Based on vkguide.net - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Engine class

#include "phvk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "phvk_types.h"
#include "phvk_initializers.h"

#include <VkBootstrap.h> 

#include <array>
#include <iostream>
#include <fstream>

#include "imgui.h"
//#include "imgui_impl_sdl2.h"
//#include "imgui_impl_vulkan.h"

#define VMA_IMPLEMENTATION  // Must include in exactly ONE cpp file
#include "vk_mem_alloc.h"   // May be included elsewhere w/o VMA_IMPLEMENTATION

#include "phvk_images.h"
#include "phvk_pipelines.h"
#include "phvk_descriptors.h"

//#include <chrono>
//#include <thread>

// Singleton instance of the engine
phVkEngine* loaded_engine = nullptr;

// Validation layers switch (for debugging)
constexpr bool use_validation_layers = true;

phVkEngine& phVkEngine::getLoadedEngine() 
{ 
    return *loaded_engine; 
}

void phVkEngine::init()
{
    // Only one engine initialization is allowed
    assert(loaded_engine == nullptr);
    loaded_engine = this;

    // Initialize SDL and create a window with it
    SDL_Init(SDL_INIT_VIDEO);

    // SDL window flags
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	// Create the window
    window = SDL_CreateWindow(
        "Acid Engine (Vulkan)",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        window_extent.width,
        window_extent.height,
        window_flags);

    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();
    initDescriptors();
	initPipelines();

    is_initialized = true;
}

void phVkEngine::cleanup()
{
    if (is_initialized) 
    {
		// Objects have interdependencies, 
        // Generally should delete in reverse order of creation

        // Wait for GPU to finish
        vkDeviceWaitIdle(device);

        // Flush the global deletion queue
		main_delete_queue.flush();

        for (int i = 0; i < FRAME_OVERLAP; i++) {
            // Note: destroying the command pool also destroys buffers allocated from it
            vkDestroyCommandPool(device, frames[i].command_pool, nullptr);

            //destroy sync objects
            vkDestroyFence(device, frames[i].render_fence, nullptr);
            vkDestroySemaphore(device, frames[i].render_semaphore, nullptr);
            vkDestroySemaphore(device, frames[i].swapchain_semaphore, nullptr);

			frames[i].delete_queue.flush();
        }

        destroySwapchain();

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyDevice(device, nullptr);

        vkb::destroy_debug_utils_messenger(instance, debug_messenger);
        vkDestroyInstance(instance, nullptr);

        SDL_DestroyWindow(window);
    }

    // Clear engine pointer
    loaded_engine = nullptr;
}

void phVkEngine::draw()
{
    // Using vkinit and vkutil functions below, naming style differs


    // *** Setup ***

    // GPU render fence wait (timeout 1s)
    VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame().render_fence, true, 1000000000));

    // Delete objects from the last frame
    getCurrentFrame().delete_queue.flush();

    // Reset render fence
    VK_CHECK(vkResetFences(device, 1, &getCurrentFrame().render_fence));

    // Request an image from the swapchain (timeout 1s)
    uint32_t swapchain_img_index;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1000000000, 
        getCurrentFrame().swapchain_semaphore, nullptr, &swapchain_img_index));

    // Command buffer
    VkCommandBuffer cmd = getCurrentFrame().main_command_buffer;

    // Reset command buffer for fresh recording
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // Configure command buffer for recording (single use)
    VkCommandBufferBeginInfo cmd_begin_info = 
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);



    // *** Recording ***

    draw_extent.width = draw_image.extent.width;
    draw_extent.height = draw_image.extent.height;

    // Start recording
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    // Transition swapchain image to writeable layout
	// Will be overwritten so older layout doesn't matter
    vkutil::transition_image(cmd, draw_image.image, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // Render the draw image
    drawBackground(cmd);

	// Transition draw and swapchain image into their respective transfer layouts
    vkutil::transition_image(cmd, draw_image.image, 
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, swapchain_images[swapchain_img_index], 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy draw image to swapchain image
    vkutil::copy_image_to_image(cmd, draw_image.image, 
        swapchain_images[swapchain_img_index], draw_extent, swapchain_extent);

    // Change the swapchain image layout to presentable mode
    vkutil::transition_image(cmd, swapchain_images[swapchain_img_index], 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // End command buffer recording
    VK_CHECK(vkEndCommandBuffer(cmd));




    // *** Present ***

    // Setup queue submission configurations
    VkCommandBufferSubmitInfo cmd_info = vkinit::command_buffer_submit_info(cmd);
    VkSemaphoreSubmitInfo wait_info = 
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, 
        getCurrentFrame().swapchain_semaphore); // Wait on swapchain semaphore
    VkSemaphoreSubmitInfo signal_info = 
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
        getCurrentFrame().render_semaphore);    // Signal render semaphore when done
    VkSubmitInfo2 submit = vkinit::submit_info(&cmd_info, &signal_info, &wait_info);

    // Submit command buffer to the queue for execution
    // The render fence will now block until the graphics commands finish execution
    VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit, getCurrentFrame().render_fence));

    // Prepare present configurations
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.pSwapchains = &swapchain;
    present_info.swapchainCount = 1;
    present_info.pWaitSemaphores = &getCurrentFrame().render_semaphore; // Wait on render semaphore
    present_info.waitSemaphoreCount = 1;
    present_info.pImageIndices = &swapchain_img_index;

	// Present the image to the window
    VK_CHECK(vkQueuePresentKHR(graphics_queue, &present_info));

    // Increment frame count
    frame_number++;
}

void phVkEngine::drawBackground(VkCommandBuffer cmd)
{
    // Flash screen over a 120 frame period
    VkClearColorValue clear_color_value;
    float flash = std::abs(std::sin(frame_number / 120.f));
    clear_color_value = { { 0.0f, 0.0f, flash, 1.0f } };

    // Configure image range object
    VkImageSubresourceRange clear_range = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);


    // *** Gradient Comput Shader ***
    
    // Bind compute pipeline - gradient shader
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradient_pipeline);

    // Bind descriptor set containing the compute draw image
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradient_pipeline_layout, 0, 1, &draw_image_descriptors, 0, nullptr);

    // Dispatch compute pipeline
    // Using a 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(draw_extent.width / 16.0), std::ceil(draw_extent.height / 16.0), 1);
}

void phVkEngine::run()
{
    SDL_Event sdl_event;
    bool sdl_quit = false;

    // main loop
    while (!sdl_quit) {

        // Handle queued events
        while (SDL_PollEvent(&sdl_event) != 0) {
            
			// Close the window on user close action (Alt+F4, X button, etc.)
            if (sdl_event.type == SDL_QUIT)
                sdl_quit = true;

            // Handle minimize and restore events
            if (sdl_event.type == SDL_WINDOWEVENT) {
                if (sdl_event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (sdl_event.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        // Do not draw if minimized
        if (stop_rendering) 
        {
            // Sleep thread
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}


void phVkEngine::createSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchain_builder{ physical_device, device, surface };

    swapchain_img_format = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchain_builder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = swapchain_img_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        //use vsync present mode
        .set_desired_present_mode(
            //VK_PRESENT_MODE_IMMEDIATE_KHR
            //VK_PRESENT_MODE_MAILBOX_KHR
            VK_PRESENT_MODE_FIFO_KHR
            //VK_PRESENT_MODE_FIFO_RELAXED_KHR
            )
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    swapchain_extent = vkbSwapchain.extent;
    //store swapchain and its related images
    swapchain = vkbSwapchain.swapchain;
    swapchain_images = vkbSwapchain.get_images().value();
    swapchain_image_views = vkbSwapchain.get_image_views().value();
}


void phVkEngine::destroySwapchain()
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    // Destroy swapchain resources
    for (int i = 0; i < swapchain_image_views.size(); i++) 
    {
        vkDestroyImageView(device, swapchain_image_views[i], nullptr);
    }
}


void phVkEngine::initVulkan()
{
    // *** Init Instance ***
    vkb::InstanceBuilder builder;

    // Create the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Acid Engine Vulkan Application")
        .request_validation_layers(use_validation_layers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // Grab the instance 
    instance = vkb_inst.instance;
    debug_messenger = vkb_inst.debug_messenger;


    // *** Init Device ***
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
    vkb::Device vkb_device = device_builder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    device = vkb_device.device;
    physical_device = vkb_physical_device.physical_device;

    
    // *** Init Queue ***
    // Use vkbootstrap to get a graphics queue
    graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();


    // *** Init Vulkan Memory Allocator (VMA) ***
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.instance = instance;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_info, &allocator);

    // Add memory allocator to delete queue
    main_delete_queue.push_function([&]()
        {
            vmaDestroyAllocator(allocator);
        });

}

// Initializes both the swapchain and Vulkan drawing image
void phVkEngine::initSwapchain()
{
    // *** Init Swapchain ***
    createSwapchain(window_extent.width, window_extent.height);



	// *** Init Draw Image ***
    
    // Draw image size will match the window
    VkExtent3D draw_image_extent = 
    {
        window_extent.width,
        window_extent.height,
        1
    };

    // Draw format hardcoded
    // 64 bits per pixel
    // May be overkill, but useful in some cases
    draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    draw_image.extent = draw_image_extent;

    // Define image usages
    VkImageUsageFlags draw_image_usages{};
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(draw_image.format, draw_image_usages, draw_image_extent);

    // Allocate draw image from GPU local memory
    // Configure for GPU-only access and fastest memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the image
    vmaCreateImage(allocator, &rimg_info, &rimg_allocinfo, &draw_image.image, &draw_image.allocation, nullptr);

    // Build an image-view for the draw image to use for rendering
    // vkguide.dev always pairs vkimages with "default" imageview
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(draw_image.format, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &draw_image.view));

    // Add to deletion functions to queue
    main_delete_queue.push_function([=]() 
        {
            vkDestroyImageView(device, draw_image.view, nullptr);
            vmaDestroyImage(allocator, draw_image.image, draw_image.allocation);
        });
}

void phVkEngine::initCommands()
{
	// The tutorial recommends simplifying this code bu using vkinit functions
    // See: https://vkguide.dev/docs/new_chapter_1/vulkan_commands_code/
    // However, I have opted to leave things more transparent for now

    // Create a command pool for commands submitted to the graphics queue
    // Configure to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo command_pool_info = {}; // Important to init to zero
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.pNext = nullptr;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_info.queueFamilyIndex = graphics_queue_family;

    for (int i = 0; i < FRAME_OVERLAP; i++) 
    {
        VK_CHECK(vkCreateCommandPool(device, &command_pool_info, nullptr, &frames[i].command_pool));

        // Allocate the default command buffer for rendering
        VkCommandBufferAllocateInfo cmd_alloc_info = {};
        cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc_info.pNext = nullptr;
        cmd_alloc_info.commandPool = frames[i].command_pool;
        cmd_alloc_info.commandBufferCount = 1;
        cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &frames[i].main_command_buffer));
    }
}

void phVkEngine::initSyncStructures()
{
    // Create CPU/GPU sync structures
    // One fence to signal when the gpu has finished rendering the frame
    // Two semaphores to synchronize rendering with swapchain
    
    // Uses vkinit functions, which is why the naming style is different
    // VK_FENCE_CREATE_SIGNALED_BIT - fence will start signaled to avoid blocking
    VkFenceCreateInfo fence_create_info = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphore_create_info = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &frames[i].render_fence));

        VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &frames[i].swapchain_semaphore));
        VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &frames[i].render_semaphore));
    }
}

void phVkEngine::initDescriptors()
{
    // Descriptor pool: 10 sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = 
        { { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 } };
    global_descriptor_allocator.init_pool(device, 10, sizes);

    // Descriptor set layout - compute draw image
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    draw_image_descriptor_layout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);

	// Allocate descriptor set - compute draw image
    draw_image_descriptors = global_descriptor_allocator.allocate(device, draw_image_descriptor_layout);

	// Update descriptor set info - compute draw image
    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = draw_image.view;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;
    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = draw_image_descriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);

    // Add delete functions to queue for descriptor allocator and layout
    main_delete_queue.push_function([&]() 
        {
            global_descriptor_allocator.destroy_pool(device);
            vkDestroyDescriptorSetLayout(device, draw_image_descriptor_layout, nullptr);
        });
}

void phVkEngine::initPipelines()
{
	initBackgroundPipelines();
}

void phVkEngine::initBackgroundPipelines()
{
	// Skipping push-constants and other special configurations - not needed for background

    // *** Pipeline Layout ***
    VkPipelineLayoutCreateInfo compute_layout{};
    compute_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    compute_layout.pNext = nullptr;
    compute_layout.pSetLayouts = &draw_image_descriptor_layout;
    compute_layout.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &compute_layout, nullptr, &gradient_pipeline_layout));


    // *** Pipeline Module ***
    // Load shader code
    VkShaderModule compute_draw_shader;
    // TODO: abstract file paths (though this one is likely temporary anyway?)
    if (!vkutil::load_shader_module("../../../../shaders/gradient.comp.spv", device, &compute_draw_shader))
    {
        fmt::print("Error when building the compute shader \n");
    }

    // Configure
    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.pNext = nullptr;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = compute_draw_shader;
    stage_info.pName = "main";  // Shader entry function name


    // *** Pipeline Creation ***
    VkComputePipelineCreateInfo compute_pipeline_create_info{};
    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.pNext = nullptr;
    compute_pipeline_create_info.layout = gradient_pipeline_layout;
    compute_pipeline_create_info.stage = stage_info;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &gradient_pipeline));


    // *** Cleanup ***
    // No longer need the module once the pipeline is created
    vkDestroyShaderModule(device, compute_draw_shader, nullptr);

	// Add delete functions to queue for pipeline and layout
    main_delete_queue.push_function([&]() 
        {
            vkDestroyPipelineLayout(device, gradient_pipeline_layout, nullptr);
            vkDestroyPipeline(device, gradient_pipeline, nullptr);
        });

}
