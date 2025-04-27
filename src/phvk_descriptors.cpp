// Written by : Patrick Marsceill (vkguide.dev)
// Used with permission - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Descriptors boilerplate 


#include "phvk_descriptors.h"
#include "phvk_initializers.h"

//> descriptor_bind
void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind {};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}
//< descriptor_bind

//> descriptor_layout
VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shader_stages, void* p_next, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shader_stages;
    }

    VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.pNext = p_next;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}
//< descriptor_layout

//> descriptor_pool_init
void DescriptorAllocator::initPool(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (PoolSizeRatio ratio : pool_ratios) {
        pool_sizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * max_sets)
        });
    }

	VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pool_info.flags = 0;
	pool_info.maxSets = max_sets;
	pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
	pool_info.pPoolSizes = pool_sizes.data();

	vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clearDescriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroyPool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}
//< descriptor_pool_init
//> descriptor_alloc
VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &ds));

    return ds;
}
//< descriptor_alloc
//> write_image
void DescriptorWriter::writeImage(int binding,VkImageView image, VkSampler sampler,  VkImageLayout layout, VkDescriptorType type)
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
//< write_image
// 
//> write_buffer
void DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
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
//< write_buffer
//> writer_end
void DescriptorWriter::clear()
{
    image_info.clear();
    writes.clear();
    buffer_info.clear();
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes) 
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}
//< writer_end
//> growpool_2
void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios)
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

void DescriptorAllocatorGrowable::clearPools(VkDevice device)
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

void DescriptorAllocatorGrowable::destroyPools(VkDevice device)
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
//< growpool_2

//> growpool_1
VkDescriptorPool DescriptorAllocatorGrowable::getPool(VkDevice device)
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

VkDescriptorPool DescriptorAllocatorGrowable::createPool(VkDevice device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios)
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
//< growpool_1

//> growpool_3
VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* p_next)
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
//< growpool_3