// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Descriptors management

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

// TODO: replace vector with ArrayList and write replacement for deque and span
#include <vector>
#include <deque>
#include <span>

#include <vulkan/vulkan.h>

#include "phVkTypes.hpp"


struct phVkDescriptorLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void addBinding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shader_stages, 
        void* p_next = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct phVkDescriptorWriter
{
    std::deque<VkDescriptorImageInfo> image_info;
    std::deque<VkDescriptorBufferInfo> buffer_info;
    std::vector<VkWriteDescriptorSet> writes;

    void writeImage(int binding, VkImageView image, VkSampler sampler, 
        VkImageLayout layout, VkDescriptorType type);
    void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, 
        VkDescriptorType type);

    void clear();
    void updateSet(VkDevice device, VkDescriptorSet set);
};

struct phVkDescriptorAllocator
{
public:
    struct PoolSizeRatio
    {
        VkDescriptorType type;
        float ratio;
    };

    void init(VkDevice device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios);
    void clearPools(VkDevice device);
    void destroyPools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* p_next = nullptr);
private:
    VkDescriptorPool getPool(VkDevice device);
    VkDescriptorPool createPool(VkDevice device, uint32_t set_count, 
        std::span<PoolSizeRatio> pool_ratios);

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> full_pools;
    std::vector<VkDescriptorPool> ready_pools;
    uint32_t sets_per_pool;
};

void phVkDescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind {};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);
}

void phVkDescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout phVkDescriptorLayoutBuilder::build(VkDevice device, 
    VkShaderStageFlags shader_stages, void* p_next, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) 
    {
        b.stageFlags |= shader_stages;
    }

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
    };
    info.pNext = p_next;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}


void phVkDescriptorWriter::writeImage(int binding,VkImageView image, VkSampler sampler,  
    VkImageLayout layout, VkDescriptorType type)
{
    VkDescriptorImageInfo& info = image_info.emplace_back(VkDescriptorImageInfo 
        {
		    .sampler = sampler,
		    .imageView = image,
		    .imageLayout = layout
	    });

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; // Left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}


void phVkDescriptorWriter::writeBuffer(int binding, VkBuffer buffer, 
    size_t size, size_t offset, VkDescriptorType type)
{
	VkDescriptorBufferInfo& info = buffer_info.emplace_back(VkDescriptorBufferInfo
        {
		    .buffer = buffer,
		    .offset = offset,
		    .range = size
		});

	VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; // Left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	writes.push_back(write);
}


void phVkDescriptorWriter::clear()
{
    image_info.clear();
    writes.clear();
    buffer_info.clear();
}


void phVkDescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes) 
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}


void phVkDescriptorAllocator::init(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios)
{
    ratios.clear();
    
    for (auto r : pool_ratios) 
    {
        ratios.push_back(r);
    }
	
    VkDescriptorPool new_pool = createPool(device, max_sets, pool_ratios);

    sets_per_pool = max_sets * 1.5; // Grow it next allocation

    ready_pools.push_back(new_pool);
}


void phvkDescriptorAllocator::clearPools(VkDevice device)
{ 
    for (auto p : ready_pools) 
    {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : full_pools) 
    {
        vkResetDescriptorPool(device, p, 0);
        ready_pools.push_back(p);
    }
    full_pools.clear();
}


void phVkDescriptorAllocator::destroyPools(VkDevice device)
{
	for (auto p : ready_pools) 
    {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
    ready_pools.clear();
	for (auto p : full_pools) 
    {
		vkDestroyDescriptorPool(device,p,nullptr);
    }
    full_pools.clear();
}


VkDescriptorPool phVkDescriptorAllocator::getPool(VkDevice device)
{       
    VkDescriptorPool new_pool;
    if (ready_pools.size() != 0) 
    {
        new_pool = ready_pools.back();
        ready_pools.pop_back();
    }
    else 
    {
	    //need to create a new pool
	    new_pool = createPool(device, sets_per_pool, ratios);

	    sets_per_pool = sets_per_pool * 1.5;
	    if (sets_per_pool > 4092) 
        {
		    sets_per_pool = 4092;
	    }
    }   

    return new_pool;
}


VkDescriptorPool phVkDescriptorAllocator::createPool(VkDevice device, 
    uint32_t set_count, std::span<PoolSizeRatio> pool_ratios)
{
	std::vector<VkDescriptorPoolSize> pool_sizes;
	for (PoolSizeRatio ratio : pool_ratios) 
    {
		pool_sizes.push_back(VkDescriptorPoolSize
            {
			    .type = ratio.type,
			    .descriptorCount = uint32_t(ratio.ratio * set_count)
		    });
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = set_count;
	pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
	pool_info.pPoolSizes = pool_sizes.data();

	VkDescriptorPool new_pool;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &new_pool);
    return new_pool;
}


VkDescriptorSet phVkDescriptorAllocator::allocate(VkDevice device, 
    VkDescriptorSetLayout layout, void* p_next)
{
    //get or create a pool to allocate from
    VkDescriptorPool pool_to_use = getPool(device);

	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.pNext = p_next;
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = pool_to_use;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &layout;

	VkDescriptorSet ds;
	VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &ds);

    //allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) 
    {
        full_pools.push_back(pool_to_use);
    
        pool_to_use = getPool(device);
        alloc_info.descriptorPool = pool_to_use;

        VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &ds));
    }
  
    ready_pools.push_back(pool_to_use);
    return ds;
}
