// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Vulkan types / structs

// Based on vkguide.dev
// https://github.com/vblanco20-1/vulkan-guide/
// 
// Original license included here:
//
// The MIT License(MIT)
//
// Copyright(c) 2016 Patrick Marsceill
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include <iostream>
#include <vector>
#include <deque>
#include <functional>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <vk_mem_alloc.h>


#define VK_CHECK(x)                                                     \
    do                                                                  \
    {                                                                   \
        VkResult err = x;                                               \
        if (err)                                                        \
        {                                                               \
            std::cout << "Detected Vulkan error: {}" << string_VkResult(err) << std::endl; \
            abort();                                                    \
        }                                                               \
    } while (0)


#include "phVkDescriptors.hpp" // Causes include loop

struct phVkImage
{
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;
};

// Swapchain frame data
struct phVkFrameData
{
    VkSemaphore swapchain_semaphore, render_semaphore;
    VkFence render_fence;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;

    phVkDescriptorAllocator frame_descriptors;

    //phVkDeleteQueue delete_queue;
};


struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};


template <typename T>
struct GPUSceneData
{
    Mat4<T> view;
    Mat4<T> proj;
    Mat4<T> view_proj;
    Vec4<T> ambient_color;
    Vec4<T> sunlight_direction; // w for sun power
    Vec4<T> sunlight_color;
};


//// Queue for deleting objects in FIFO order
//struct phVkDeleteQueue
//{
//    // TODO: replace std:: functions?
//    std::deque<std::function<void()>> fifo;
//
//    void pushFunction(std::function<void()>&& function)
//    {
//        fifo.push_back(function);
//    }
//
//    void flush()
//    {
//        // Reverse iterate the deletion queue to execute all the functions
//        for (auto it = fifo.rbegin(); it != fifo.rend(); it++)
//        {
//            (*it)(); // Call function
//        }
//
//        fifo.clear();
//    }
//};
