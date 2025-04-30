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
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#define VMA_IMPLEMENTATION  // Must include in exactly ONE cpp file
#include <vk_mem_alloc.h>   // May be included elsewhere w/o VMA_IMPLEMENTATION

#include "phvk_images.h"
#include "phvk_pipelines.h"
#include "phvk_descriptors.h"

#include "vec.hpp"
#include "math_misc.hpp"

//#include <chrono>
//#include <thread>

// Singleton instance of the engine
phVkEngine* loaded_engine = nullptr;

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
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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
    initImgui();
    initDefaultData();

    // Camera
    main_camera.velocity = Vec3f();
	main_camera.position = Vec3f(0.f, 0.f, 5.f);
    main_camera.pitch = 0.f;
	main_camera.yaw = 0.f;

    is_initialized = true;
}

void phVkEngine::run()
{
    SDL_Event sdl_event;
    bool sdl_quit = false;

    // main loop
    while (!sdl_quit) {

        // Handle queued events
        while (SDL_PollEvent(&sdl_event) != 0)
        {
            // Close the window on user close action (Alt+F4, X button, etc.)
            if (sdl_event.type == SDL_QUIT)
                sdl_quit = true;

            // Handle minimize and restore events
            if (sdl_event.type == SDL_WINDOWEVENT)
            {
                if (sdl_event.window.event == SDL_WINDOWEVENT_MINIMIZED)
                {
                    stop_rendering = true;
                }
                if (sdl_event.window.event == SDL_WINDOWEVENT_RESTORED)
                {
                    stop_rendering = false;
                }
            }

            // Handle camera movement
            main_camera.processSDLEvent(sdl_event);

            // Send SDL event to imgui for handling
            ImGui_ImplSDL2_ProcessEvent(&sdl_event);
        }

        // Do not draw if minimized
        if (stop_rendering)
        {
            // Sleep thread
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Window resize check
		if (resize_requested)
		{
			resizeSwapchain();
			resize_requested = false;
		}

        // Imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Imgui window for controlling background
        if (ImGui::Begin("background"))
        {
            ImGui::SliderFloat("Render Scale", &render_scale, 0.3f, 1.f);

            ComputeEffect& selected = background_effects[current_background_effect];

            ImGui::Text("Selected effect: ", selected.name);

            ImGui::SliderInt("Effect Index", &current_background_effect, 0, background_effects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);

            ImGui::End();
        }

        // Calculate internal draw structures for imgui (does not draw to a Vulkan image)
        ImGui::Render();

        draw();
    }
}

void phVkEngine::cleanup()
{
    if (is_initialized) 
    {
		// Objects have interdependencies, 
        // Generally should delete in reverse order of creation

        // Wait for GPU to finish
        vkDeviceWaitIdle(device);

        for (int i = 0; i < FRAME_OVERLAP; i++) {
            // Note: destroying the command pool also destroys buffers allocated from it
            vkDestroyCommandPool(device, frames[i].command_pool, nullptr);

            //destroy sync objects
            vkDestroyFence(device, frames[i].render_fence, nullptr);
            vkDestroySemaphore(device, frames[i].render_semaphore, nullptr);
            vkDestroySemaphore(device, frames[i].swapchain_semaphore, nullptr);

			frames[i].delete_queue.flush();
        }

        // Destroy mesh array buffers
        for (auto& mesh : test_meshes)
        {
            destroyBuffer(mesh->mesh_buffers.index_buffer);
            destroyBuffer(mesh->mesh_buffers.vertex_buffer);
        }

        metal_rough_material.clearResources(device);

        // Flush the global deletion queue
        main_delete_queue.flush();

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

    // Scene nodes
    updateScene();

    // *** Setup ***
    //
    // GPU render fence wait (timeout 1s)
    VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame().render_fence, true, 1000000000));

    // Flush frame data
    getCurrentFrame().delete_queue.flush();
    getCurrentFrame().frame_descriptors.clearPools(device);

    // Delete objects from the last frame
    getCurrentFrame().delete_queue.flush();

    // Request an image from the swapchain (timeout 1s)
    uint32_t swapchain_img_index;
    VkResult e = vkAcquireNextImageKHR(device, swapchain, 1000000000, 
        getCurrentFrame().swapchain_semaphore, nullptr, &swapchain_img_index);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        resize_requested = true;    // Window resize, rebuild pipeline
        return;
    }

    // Draw extent
    draw_extent.height = std::min(swapchain_extent.height, draw_image.extent.height) * render_scale;
    draw_extent.width = std::min(swapchain_extent.width, draw_image.extent.width) * render_scale;


    // Reset render fence
    VK_CHECK(vkResetFences(device, 1, &getCurrentFrame().render_fence));

    // Command buffer
    VkCommandBuffer cmd = getCurrentFrame().main_command_buffer;

    // Reset command buffer for fresh recording
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // Configure command buffer for recording (single use)
    VkCommandBufferBeginInfo cmd_begin_info = 
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);



    // *** Recording ***

    // Start recording
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    // Transition swapchain image to writeable layout
	// Will be overwritten so older layout doesn't matter
    vkutil::transition_image(cmd, draw_image.image, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // Render the draw image
    drawBackground(cmd);

    // Optimize layout for geometry rendering
    vkutil::transition_image(cmd, draw_image.image, 
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Transition depth image into depth attachment mode
    vkutil::transition_image(cmd, depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    drawGeometry(cmd);

	// Transition draw and swapchain image into their respective transfer layouts
    vkutil::transition_image(cmd, draw_image.image, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, swapchain_images[swapchain_img_index], 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy draw image to swapchain image
    vkutil::copy_image_to_image(cmd, draw_image.image, 
        swapchain_images[swapchain_img_index], draw_extent, swapchain_extent);

    // Imgui: change swapchain image layout to "Attachment Optimal" for imgui drawing
    vkutil::transition_image(cmd, swapchain_images[swapchain_img_index], 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Imgui: draw into the swapchain image
    drawImgui(cmd, swapchain_image_views[swapchain_img_index]);

    // Change the swapchain image layout to presentable mode
    vkutil::transition_image(cmd, swapchain_images[swapchain_img_index], 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
    VkResult present_result = vkQueuePresentKHR(graphics_queue, &present_info);

    // TODO: this isn't in the chapter 4 code
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        resize_requested = true;    // Window resize, rebuild pipeline
    }

    // Increment frame count
    frame_number++;
}

void phVkEngine::drawBackground(VkCommandBuffer cmd)
{
    // Background effect for tutorial
    ComputeEffect& effect = background_effects[current_background_effect];

    // Bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradient_pipeline_layout, 0, 1, &draw_image_descriptors, 0, nullptr);

    vkCmdPushConstants(cmd, gradient_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // Execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(draw_extent.width / 16.0), std::ceil(draw_extent.height / 16.0), 1);
}

void phVkEngine::drawGeometry(VkCommandBuffer cmd)
{
    // *** Setup ***
    // 
    // Attach draw image
    VkRenderingAttachmentInfo color_attachment = vkinit::attachment_info(draw_image.view,
        nullptr, VK_IMAGE_LAYOUT_GENERAL);

    // Attach depth image
    VkRenderingAttachmentInfo depth_attachment = vkinit::depth_attachment_info(depth_image.view,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    VkRenderingInfo render_info = vkinit::rendering_info(window_extent,
        &color_attachment, &depth_attachment);

    // Begin render pass
    vkCmdBeginRendering(cmd, &render_info);

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline);

    // Set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = draw_extent.width;
    viewport.height = draw_extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = viewport.width;
    scissor.extent.height = viewport.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Allocate a new uniform buffer for the scene data
    AllocatedBuffer gpu_scene_data_buffer = createBuffer(sizeof(GPUSceneData), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // Add it to the deletion queue of this frame so it gets deleted once its been used
    getCurrentFrame().delete_queue.pushFunction([=, this]() 
        {
            destroyBuffer(gpu_scene_data_buffer);
        });

    // Write the buffer
    GPUSceneData* sceneUniformData = (GPUSceneData*)gpu_scene_data_buffer.allocation->GetMappedData();
    *sceneUniformData = scene_data;

    // Vreate a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor = getCurrentFrame().frame_descriptors.allocate(
        device, gpu_scene_data_descriptor_layout);

    // Descriptor writer
    DescriptorWriter writer;
    writer.writeBuffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0, 
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.updateSet(device, globalDescriptor);


    // Draw scene nodes meshes
    for (const RenderObject& draw : main_draw_context.opaque_surfaces) 
    {
        // TODO binding every draw is inefficient - will fix later in tutorial
        
        // Bind material pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->pipeline);
        // Bind global descriptors
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 
            0, 1, &globalDescriptor, 0, nullptr);
        // Bind material descriptors
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 
            1, 1, &draw.material->material_set, 0, nullptr);

        vkCmdBindIndexBuffer(cmd, draw.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        GPUDrawPushConstants push_constants;
        push_constants.vertex_buffer_address = draw.vertex_buffer_address;
        push_constants.world_matrix = draw.transform;
        vkCmdPushConstants(cmd, draw.material->pipeline->layout, 
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

        vkCmdDrawIndexed(cmd, draw.index_count, 1, draw.first_index, 0, 0);
    }

    vkCmdEndRendering(cmd);
}

void phVkEngine::drawImgui(VkCommandBuffer cmd, VkImageView target_image_view)
{
    VkRenderingAttachmentInfo color_attachment = vkinit::attachment_info(target_image_view, nullptr, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo render_info = vkinit::rendering_info(swapchain_extent, &color_attachment, nullptr);

    vkCmdBeginRendering(cmd, &render_info);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void phVkEngine::updateScene()
{
    main_draw_context.opaque_surfaces.clear();

    loaded_nodes["Suzanne"]->draw(Mat4f(), main_draw_context);

    for (int x = -3; x < 3; x++) 
    {

        Mat4f scale = Mat4f(Mat3f::scale(Vec3f(0.2, 0.2, 0.2)));
        Mat4f translation = Mat4f::transl(Vec3f(x, 1, 0));

        loaded_nodes["Cube"]->draw(translation * scale, main_draw_context);
    }

    main_camera.update();

    scene_data.view = main_camera.getViewMatrix();
    //scene_data.view = Mat4f::transl(Vec3f(0, 0, -5));

    // Camera projection
    // Note: Using reversed depth (near/far) where 1 is near and 0 is far
    //	     This is a common optimization in Vulkan to avoid depth precision issues
    scene_data.proj = Mat4f::projPerspective(1.22173f,
        (float)window_extent.width / (float)window_extent.height, 10000.f, 0.1f);

    // Invert the Y direction on projection matrix
    // glTF is designed for OpenGL which has +Y up (vs Vulkan which is +Y down)
    scene_data.proj[1][1] *= -1;
    scene_data.view_proj = scene_data.proj * scene_data.view;

    // Some default lighting parameters
    scene_data.ambient_color = Vec4f(0.1f, 0.1f, 0.1f, 0.1f);
    scene_data.sunlight_color = Vec4f(1.f, 1.f, 1.f, 1.f);
    scene_data.sunlight_direction = Vec4f(0.f, 1.f, 0.5f, 1.f);
}

void phVkEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    // Similar to executing commands on the GPU --
    // the main difference is submit is not synchronized with the swapchain
    // TODO: optimization - use use separate queue instead of graphics queue

    // Reset the fence and command buffer
    VK_CHECK(vkResetFences(device, 1, &imm_fence));
    VK_CHECK(vkResetCommandBuffer(imm_command_buffer, 0));

    VkCommandBuffer cmd = imm_command_buffer;

    VkCommandBufferBeginInfo cmd_begin_info =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmd_info = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmd_info, nullptr, nullptr);

    // Submit command buffer to the queue and execute it
    VK_CHECK(vkQueueSubmit2(graphics_queue, 1, &submit, imm_fence));

    // Wait for immediate command fence
    VK_CHECK(vkWaitForFences(device, 1, &imm_fence, true, 9999999999));

    // Now the engine will go back to waiting on render_fence
}

GPUMeshBuffers phVkEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    // Using GPU_ONLY buffers is highly recommended for mesh performance
    // Few examples of CPU / CPU-accessible buffer might be CPU-driven particle system or dynamic effects

    // TODO: Optimization - put this in a background thread so we don't..
    //      have to wait for the GPU commands to complete


    // *** Create GPU Buffers ***

    const size_t vertex_buf_size = vertices.size() * sizeof(Vertex);
    const size_t index_buf_size = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers new_mesh;

    // Create vertex buffer
    new_mesh.vertex_buffer = createBuffer(vertex_buf_size, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | // SSBO | memory copy
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Find the adress of the vertex buffer
    VkBufferDeviceAddressInfo device_addr_info{ 
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = new_mesh.vertex_buffer.buffer };
    new_mesh.vertex_buffer_address = vkGetBufferDeviceAddress(device, &device_addr_info);

    // Create index buffer
    new_mesh.index_buffer = createBuffer(index_buf_size, 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,    // Index draws | memory copy
        VMA_MEMORY_USAGE_GPU_ONLY);


	// *** Copy Data to GPU ***

    // Set up a staging buffer
    AllocatedBuffer staging = createBuffer(vertex_buf_size + index_buf_size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // Copy vertex buffer to staging
    memcpy(data, vertices.data(), vertex_buf_size);
    // Copy index buffer staging
    memcpy((char*)data + vertex_buf_size, indices.data(), index_buf_size);

	// Submit commands to copy from staging to GPU buffer
    immediateSubmit([&](VkCommandBuffer cmd) 
        {
            VkBufferCopy vertexCopy{ 0 };
            vertexCopy.dstOffset = 0;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = vertex_buf_size;

            vkCmdCopyBuffer(cmd, staging.buffer, new_mesh.vertex_buffer.buffer, 1, &vertexCopy);

            VkBufferCopy indexCopy{ 0 };
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = vertex_buf_size;
            indexCopy.size = index_buf_size;

            vkCmdCopyBuffer(cmd, staging.buffer, new_mesh.index_buffer.buffer, 1, &indexCopy);
        });

	// Destroy temporary staging buffer
    destroyBuffer(staging);

    return new_mesh;
}

AllocatedImage phVkEngine::createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage new_image;
    new_image.format = format;
    new_image.extent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped) 
    {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // Always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the image
    VK_CHECK(vmaCreateImage(allocator, &img_info, &alloc_info, &new_image.image, &new_image.allocation, nullptr));

    // If the format is a depth format, must use the correct aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) 
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // Build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, new_image.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &new_image.view));

    return new_image;
}

AllocatedImage phVkEngine::createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = createBuffer(data_size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = createImage(size, format, usage | 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediateSubmit([&](VkCommandBuffer cmd) 
        {
            vkutil::transition_image(cmd, new_image.image, 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copy_region = {};
            copy_region.bufferOffset = 0;
            copy_region.bufferRowLength = 0;
            copy_region.bufferImageHeight = 0;

            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageExtent = size;

            // copy the buffer into the image
            vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });

    destroyBuffer(uploadbuffer);

    return new_image;
}

void phVkEngine::destroyImage(const AllocatedImage& img)
{
    vkDestroyImageView(device, img.view, nullptr);
    vmaDestroyImage(allocator, img.image, img.allocation);
}

AllocatedBuffer phVkEngine::createBuffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage)
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
    VK_CHECK(vmaCreateBuffer(allocator, &buffer_info, &vmaallocInfo, &new_buffer.buffer, &new_buffer.allocation,
        &new_buffer.info));

    return new_buffer;
}

void phVkEngine::destroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
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


void phVkEngine::resizeSwapchain()
{
    // Tear down and rebulid swapchain with new window_extent

    vkDeviceWaitIdle(device);

    destroySwapchain();

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    window_extent.width = w;
    window_extent.height = h;

    createSwapchain(window_extent.width, window_extent.height);
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
    vkb::Device vkbdevice = device_builder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    device = vkbdevice.device;
    physical_device = vkb_physical_device.physical_device;

    
    // *** Init Queue ***
    // Use vkbootstrap to get a graphics queue
    graphics_queue = vkbdevice.get_queue(vkb::QueueType::graphics).value();
    graphics_queue_family = vkbdevice.get_queue_index(vkb::QueueType::graphics).value();


    // *** Init Vulkan Memory Allocator (VMA) ***
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.instance = instance;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_info, &allocator);

    // Add memory allocator to delete queue
    main_delete_queue.pushFunction([&]()
        {
            vmaDestroyAllocator(allocator);
        });

}

// Initializes both the swapchain and Vulkan drawing image
void phVkEngine::initSwapchain()
{
    // *** Init Swapchain ***
    //
    createSwapchain(window_extent.width, window_extent.height);

	// *** Init Draw Image ***
    //
    // Set the draw image to monitor dimensions
    // Will only render to a portion that matches the window size
    VkExtent3D draw_image_extent = 
    {
        // TODO: Still crashes if resizing above these dimensions
        // TODO: Pull monitor dimensions rather than hard-coding?
        window_extent.width/*2560*/,    // TOOD
        window_extent.height/*1440*/,   // TODO
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
    VmaAllocationCreateInfo rimg_alloc_info = {};
    rimg_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the image
    vmaCreateImage(allocator, &rimg_info, &rimg_alloc_info, &draw_image.image, &draw_image.allocation, nullptr);

    // Build an image-view for the draw image to use for rendering
    // vkguide.dev always pairs vkimages with "default" imageview
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(draw_image.format, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &draw_image.view));




    // *** Init Depth Image ***
    //
    depth_image.format = VK_FORMAT_D32_SFLOAT;
    depth_image.extent = draw_image_extent;
    VkImageUsageFlags depth_image_usages{};
    depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;  // Key to depth pass

    VkImageCreateInfo dimg_info = vkinit::image_create_info(depth_image.format, depth_image_usages, draw_image_extent);

    // Allocate and create the image
    vmaCreateImage(allocator, &dimg_info, &rimg_alloc_info, &depth_image.image, &depth_image.allocation, nullptr);

    // Build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(depth_image.format, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(device, &dview_info, nullptr, &depth_image.view));




    // *** Cleanup ***
    //
    // Add to deletion functions to queue
    main_delete_queue.pushFunction([=]() 
        {
            vkDestroyImageView(device, depth_image.view, nullptr);
            vmaDestroyImage(allocator, depth_image.image, depth_image.allocation);
            
            vkDestroyImageView(device, draw_image.view, nullptr);
            vmaDestroyImage(allocator, draw_image.image, draw_image.allocation);
        });
}

void phVkEngine::initCommands()
{
	// *** Command Pool ***
        
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



    // *** Immediate Commands ***

    VK_CHECK(vkCreateCommandPool(device, &command_pool_info, nullptr, &imm_command_pool));

    // Allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmd_alloc_info = 
        vkinit::command_buffer_allocate_info(imm_command_pool, 1);

    VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &imm_command_buffer));

    main_delete_queue.pushFunction([=]() 
        {
            vkDestroyCommandPool(device, imm_command_pool, nullptr);
        });
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

    // Immediate commands fence
    VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &imm_fence));

    main_delete_queue.pushFunction([=]() { vkDestroyFence(device, imm_fence, nullptr); });
}

void phVkEngine::initDescriptors()
{
    // Descriptor pool: 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = 
    { 
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } 
    };

    global_descriptor_allocator.init(device, 10, sizes);

    // Descriptor set layout - compute draw image
    {       // Block/naked scope - not sure why this is necessary
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        draw_image_descriptor_layout = builder.build(device, 
            VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        gpu_scene_data_descriptor_layout = builder.build(device, 
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        single_image_descriptor_layout = builder.build(device, 
            VK_SHADER_STAGE_FRAGMENT_BIT);
    }

	// Allocate descriptor set - compute draw image
    draw_image_descriptors = global_descriptor_allocator.allocate(device, draw_image_descriptor_layout);

	// Update descriptor set info - compute draw image
    DescriptorWriter writer;
    writer.writeImage(0, draw_image.view, VK_NULL_HANDLE, 
        VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.updateSet(device, draw_image_descriptors);

    // Add delete functions to queue for descriptor allocator and layout
    main_delete_queue.pushFunction([&]() 
        {
            global_descriptor_allocator.destroyPools(device);

            vkDestroyDescriptorSetLayout(device, draw_image_descriptor_layout, nullptr);
            vkDestroyDescriptorSetLayout(device, single_image_descriptor_layout, nullptr);
            vkDestroyDescriptorSetLayout(device, gpu_scene_data_descriptor_layout, nullptr);
        });


	// Initialize frame descriptor pool
    for (int i = 0; i < FRAME_OVERLAP; i++) 
    {
        // Create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = 
            {
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
            };

        frames[i].frame_descriptors = DescriptorAllocatorGrowable{};
        frames[i].frame_descriptors.init(device, 1000, frame_sizes);

        // Delete queue
        main_delete_queue.pushFunction([&, i]() 
            {
                frames[i].frame_descriptors.destroyPools(device);
            });
    }
}

void phVkEngine::initPipelines()
{
    // Compute Pipelines
    initBackgroundPipelines();

	// Graphics Pipelines
	initMeshPipeline();

    metal_rough_material.buildPipelines(this);
}

void phVkEngine::initBackgroundPipelines()
{
    // *** Pipeline Layout ***

    VkPipelineLayoutCreateInfo compute_layout{};
    compute_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    compute_layout.pNext = nullptr;
    compute_layout.pSetLayouts = &draw_image_descriptor_layout;
    compute_layout.setLayoutCount = 1;

    // Add push constants
    VkPushConstantRange push_constant{};
    push_constant.offset = 0;
    push_constant.size = sizeof(ComputePushConstants);
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_layout.pPushConstantRanges = &push_constant;
    compute_layout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &compute_layout, nullptr, &gradient_pipeline_layout));




    // *** Pipeline Module ***

    // Load shader code
    VkShaderModule gradient_shader;
    // TODO: abstract file paths (though this one is likely temporary anyway?)
    if (!vkutil::load_shader_module("../../../../shaders/gradient_color.comp.spv", device, &gradient_shader))
    {
        fmt::print("Error when building the gradient shader \n");
    }

    // Load shader code
    VkShaderModule sky_shader;
    // TODO: abstract file paths (though this one is likely temporary anyway?)
    if (!vkutil::load_shader_module("../../../../shaders/sky.comp.spv", device, &sky_shader))
    {
        fmt::print("Error when building the sky shader \n");
    }

    // Configure gradient
    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.pNext = nullptr;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = gradient_shader;
    stage_info.pName = "main";  // Shader entry function name



    // *** Pipeline Creation ***
    VkComputePipelineCreateInfo compute_pipeline_create_info{};
    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.pNext = nullptr;
    compute_pipeline_create_info.layout = gradient_pipeline_layout;
    compute_pipeline_create_info.stage = stage_info;

    ComputeEffect gradient;
    gradient.layout = gradient_pipeline_layout;
    gradient.name = "gradient";
    gradient.data = {};

    // Default colors
    gradient.data.data1 = Vec4f(1, 0, 0, 1);
    gradient.data.data2 = Vec4f(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &gradient.pipeline));

    // Change the shader module only to create the sky shader
    compute_pipeline_create_info.stage.module = sky_shader;

    ComputeEffect sky;
    sky.layout = gradient_pipeline_layout;
    sky.name = "sky";
    sky.data = { Vec4f() };
    // Default sky parameters
    sky.data.data1 = Vec4f(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &sky.pipeline));

    // Add the two background effects into the array
    background_effects.push_back(gradient);
    background_effects.push_back(sky);



    // *** Cleanup ***
    // No longer need the module once the pipeline is created
    vkDestroyShaderModule(device, gradient_shader, nullptr);
    vkDestroyShaderModule(device, sky_shader, nullptr);

    // Add delete functions to queue for pipeline and layout
    main_delete_queue.pushFunction([=]()
        {
            vkDestroyPipelineLayout(device, gradient_pipeline_layout, nullptr);
            vkDestroyPipeline(device, sky.pipeline, nullptr);
            vkDestroyPipeline(device, gradient.pipeline, nullptr);
        });
}

void phVkEngine::initMeshPipeline()
{
	// *** Pipeline Module ***
    VkShaderModule triangle_frag_shader;
    if (!vkutil::load_shader_module("../../../../shaders/tex_image.frag.spv", 
        device, &triangle_frag_shader)) 
    {
        fmt::print("Error when building the fragment shader \n");
    }
    else 
    {
        fmt::print("Fragment shader successfully loaded \n");
    }

    VkShaderModule triangle_vertex_shader;
    if (!vkutil::load_shader_module("../../../../shaders/colored_triangle_mesh.vert.spv", 
        device, &triangle_vertex_shader))
    {
        fmt::print("Error when building the vertex shader \n");
    }
    else 
    {
        fmt::print("Vertex shader successfully loaded \n");
    }

    VkPushConstantRange buffer_range{};
    buffer_range.offset = 0;
    buffer_range.size = sizeof(GPUDrawPushConstants);
    buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;


    // *** Pipeline Layout ***
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &buffer_range;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pSetLayouts = &single_image_descriptor_layout;
    pipeline_layout_info.setLayoutCount = 1;


    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &mesh_pipeline_layout));



	// *** Pipeline Creation ***
    PipelineBuilder pipeline_builder;

    // Pipeline layout
    pipeline_builder.pipeline_layout = mesh_pipeline_layout;
    // Connect the vertex and pixel shaders to the pipeline
    pipeline_builder.setShaders(triangle_vertex_shader, triangle_frag_shader);
    // Topology to draw triangles
    pipeline_builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // Filled triangles
    pipeline_builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    // No backface culling
    pipeline_builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // No multisampling
    pipeline_builder.setMultiSamplingNone();

    // Blending
    pipeline_builder.disableBlending();
    //pipeline_builder.enableBlendingAdditive();

	// Depth testing
    //pipeline_builder.disableDepthtest();
    // Note: Using reversed depth (near/far) where 1 is near and 0 is far
    //	     This is a common optimization in Vulkan to avoid depth precision issues
    pipeline_builder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // Connect the image format we will draw into, from draw image
    pipeline_builder.setColorAttachmentFormat(draw_image.format);
    pipeline_builder.setDepthFormat(depth_image.format);

    // Build the pipeline
    mesh_pipeline = pipeline_builder.buildPipeline(device);



	// *** Cleanup ***
    //clean structures
    vkDestroyShaderModule(device, triangle_frag_shader, nullptr);
    vkDestroyShaderModule(device, triangle_vertex_shader, nullptr);

    // Add delete queue functions
    main_delete_queue.pushFunction([&]() 
        {
            vkDestroyPipelineLayout(device, mesh_pipeline_layout, nullptr);
            vkDestroyPipeline(device, mesh_pipeline, nullptr);
        });
}

void phVkEngine::initImgui()
{
    // *** Create Descriptor Pool ***
 
    // Greatly oversized - copied from imgui demo
    // TODO: Optimization - size down?
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imgui_pool;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &imgui_pool));



    // *** Initialize Imgui Library ***

    // Initialize the core structures of imgui
    ImGui::CreateContext();

    // Initialize imgui for SDL2
    ImGui_ImplSDL2_InitForVulkan(window);

    // Initialize imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.Queue = graphics_queue;
    init_info.DescriptorPool = imgui_pool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    // Dynamic rendering parameters for imgui
    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain_img_format;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();



    // *** Deletion Queue ***

	// Add destroy functions to the deletion queue
    main_delete_queue.pushFunction([=]() 
        {
            ImGui_ImplVulkan_Shutdown();
            vkDestroyDescriptorPool(device, imgui_pool, nullptr);
        });
}

void phVkEngine::initDefaultData() 
{
    // Three default textures: white, grey, black. One pixel each
    uint32_t white = PackFloatInt4x8(Vec4f(1, 1, 1, 1));
    white_image = createImage((void*)&white, VkExtent3D{ 1, 1, 1 }, 
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = PackFloatInt4x8(Vec4f(0.66f, 0.66f, 0.66f, 1));
    grey_image = createImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, 
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = PackFloatInt4x8(Vec4f(0, 0, 0, 0));
    black_image = createImage((void*)&black, VkExtent3D{ 1, 1, 1 }, 
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // Checkerboard image
    uint32_t magenta = PackFloatInt4x8(Vec4f(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16 > pixels; // For 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) 
    {
        for (int y = 0; y < 16; y++) 
        {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    error_checkerboard_image = createImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, 
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(device, &sampl, nullptr, &default_sampler_nearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(device, &sampl, nullptr, &default_sampler_linear);

    main_delete_queue.pushFunction([&]() 
        {
            vkDestroySampler(device, default_sampler_nearest, nullptr);
            vkDestroySampler(device, default_sampler_linear, nullptr);

            destroyImage(white_image);
            destroyImage(grey_image);
            destroyImage(black_image);
            destroyImage(error_checkerboard_image);
		});



    // *** Material Setup ***
    GLTFMetallicRoughness::MaterialResources material_resources;
    // Material textures defaults
    material_resources.color_image = white_image;
    material_resources.color_sampler = default_sampler_linear;
    material_resources.metal_rough_image = white_image;
    material_resources.metal_rough_sampler = default_sampler_linear;

    // Set the uniform buffer for the material data
    AllocatedBuffer material_constants = createBuffer(sizeof(GLTFMetallicRoughness::MaterialConstants), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // Write the buffer
    GLTFMetallicRoughness::MaterialConstants* sceneUniformData = 
        (GLTFMetallicRoughness::MaterialConstants*)material_constants.allocation->GetMappedData();
    sceneUniformData->color_factors = Vec4f{ 1, 1, 1, 1 };
    sceneUniformData->metal_rough_factors = Vec4f{ 1, 0.5, 0, 0 };

    // Cleanup
    main_delete_queue.pushFunction([=, this]() 
        {
            destroyBuffer(material_constants);
        });

    material_resources.data_buffer = material_constants.buffer;
    material_resources.data_buffer_offset = 0;

    default_data = metal_rough_material.writeMaterial(device, 
        MaterialPass::main_color, material_resources, global_descriptor_allocator);



    // *** Load Suzanne Monkey mesh ***
    // TODO: why are we not adding deletion functions here instead of manually in cleanup?
    test_meshes = loadGLTFMeshes(this, "../../../../assets/basicmesh.glb").value();


    // *** Set Up Default Meshes
    for (auto& m : test_meshes) 
    {
        std::shared_ptr<MeshNode> new_node = std::make_shared<MeshNode>();
        new_node->mesh = m;

        new_node->local_transform = Mat4f();    // Identity
        new_node->world_transform = Mat4f();    // Identity

        for (auto& s : new_node->mesh->surfaces) 
        {
            s.material = std::make_shared<GLTFMaterial>(default_data);
        }

        loaded_nodes[m->name] = std::move(new_node);
    }
}

void GLTFMetallicRoughness::buildPipelines(phVkEngine* engine)
{
    // *** Set Up Pipeline ***

    VkShaderModule mesh_frag_shader;
    if (!vkutil::load_shader_module("../../../../shaders/mesh.frag.spv", engine->device, &mesh_frag_shader)) 
    {
        fmt::println("Error when building the fragment shader module");
    }

    VkShaderModule mesh_vertex_shader;
    if (!vkutil::load_shader_module("../../../../shaders/mesh.vert.spv", engine->device, &mesh_vertex_shader)) 
    {
        fmt::println("Error when building the vertex shader module");
    }

    VkPushConstantRange matrix_range{};
    matrix_range.offset = 0;
    matrix_range.size = sizeof(GPUDrawPushConstants);
    matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layout_builder;
    layout_builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layout_builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layout_builder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    material_layout = layout_builder.build(engine->device, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = { engine->gpu_scene_data_descriptor_layout,
        material_layout };

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = &matrix_range;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout new_layout;
    VK_CHECK(vkCreatePipelineLayout(engine->device, &mesh_layout_info, nullptr, &new_layout));

    opaque_pipeline.layout = new_layout;
    transparent_pipeline.layout = new_layout;

    // Build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(mesh_vertex_shader, mesh_frag_shader);
    pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.setMultiSamplingNone();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // Render format
    pipelineBuilder.setColorAttachmentFormat(engine->draw_image.format);
    pipelineBuilder.setDepthFormat(engine->depth_image.format);

    // Yse the triangle layout we created
    pipelineBuilder.pipeline_layout = new_layout;

    // finally build the pipeline
    opaque_pipeline.pipeline = pipelineBuilder.buildPipeline(engine->device);

    // create the transparent variant
    pipelineBuilder.enableBlendingAdditive();

    pipelineBuilder.enableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparent_pipeline.pipeline = pipelineBuilder.buildPipeline(engine->device);

    vkDestroyShaderModule(engine->device, mesh_frag_shader, nullptr);
    vkDestroyShaderModule(engine->device, mesh_vertex_shader, nullptr);
}

void GLTFMetallicRoughness::clearResources(VkDevice device)
{
    vkDestroyDescriptorSetLayout(device, material_layout, nullptr);
    vkDestroyPipelineLayout(device, transparent_pipeline.layout, nullptr);

    vkDestroyPipeline(device, transparent_pipeline.pipeline, nullptr);
    vkDestroyPipeline(device, opaque_pipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallicRoughness::writeMaterial(VkDevice device, MaterialPass pass, 
    const MaterialResources& resources, DescriptorAllocatorGrowable& descriptor_allocator)
{
    MaterialInstance mat_data;
    mat_data.pass_type = pass;
    if (pass == MaterialPass::transparent) 
    {
        mat_data.pipeline = &transparent_pipeline;
    }
    else 
    {
        mat_data.pipeline = &opaque_pipeline;
    }

    mat_data.material_set = descriptor_allocator.allocate(device, material_layout);


    writer.clear();
    writer.writeBuffer(0, resources.data_buffer, sizeof(MaterialConstants), 
        resources.data_buffer_offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.writeImage(1, resources.color_image.view, resources.color_sampler, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.writeImage(2, resources.metal_rough_image.view, resources.metal_rough_sampler, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.updateSet(device, mat_data.material_set);

    return mat_data;
}

void MeshNode::draw(const Mat4f& top_matrix, DrawContext& ctx)
{
    Mat4f node_matrix = top_matrix * world_transform;

    for (auto& s : mesh->surfaces)
    {
        RenderObject def;
        def.index_count = s.count;
        def.first_index = s.start_index;
        def.index_buffer = mesh->mesh_buffers.index_buffer.buffer;
        def.material = &s.material->data;

        def.transform = node_matrix;
        def.vertex_buffer_address = mesh->mesh_buffers.vertex_buffer_address;

        ctx.opaque_surfaces.push_back(def);
    }

    // recurse down
    Node::draw(top_matrix, ctx);
}
