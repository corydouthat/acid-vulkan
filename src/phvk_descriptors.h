// Written by : Patrick Marsceill (vkguide.dev)
// Used with permission - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Descriptors boilerplate 

#pragma once

#include <vector>

#include "phvk_types.h"

#include <deque>
#include <span>

//> descriptor_layout
struct DescriptorLayoutBuilder 
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void addBinding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* p_next = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};
//< descriptor_layout
// 
//> writer
struct DescriptorWriter 
{
    std::deque<VkDescriptorImageInfo> image_info;
    std::deque<VkDescriptorBufferInfo> buffer_info;
    std::vector<VkWriteDescriptorSet> writes;

    void writeImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type); 

    void clear();
    void updateSet(VkDevice device, VkDescriptorSet set);
};
//< writer
// 
//> descriptor_allocator
struct DescriptorAllocator 
{

    struct PoolSizeRatio
    {
		VkDescriptorType type;
		float ratio;
    };

    VkDescriptorPool pool;

    void initPool(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios);
    void clearDescriptors(VkDevice device);
    void destroyPool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};
//< descriptor_allocator

//> descriptor_allocator_grow
struct DescriptorAllocatorGrowable 
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
	VkDescriptorPool createPool(VkDevice device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> full_pools;
	std::vector<VkDescriptorPool> ready_pools;
	uint32_t sets_per_pool;

};
//< descriptor_allocator_grow