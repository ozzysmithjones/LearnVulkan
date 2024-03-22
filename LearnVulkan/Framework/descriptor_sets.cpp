#include "descriptor_sets.h"
#include "error.h"

VkDescriptorPool create_descriptor_pool(VkDevice device, VkDescriptorType type, bool descriptor_sets_individually_resetable, std::size_t max_sets, std::size_t max_descriptors_per_set)
{
	VkDescriptorPool descriptor_pool{ VK_NULL_HANDLE };
	std::array<VkDescriptorPoolSize, 2> pool_sizes{};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = static_cast<uint32_t>(max_descriptors_per_set);
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = static_cast<uint32_t>(max_descriptors_per_set);

	VkDescriptorPoolCreateInfo descriptor_pool_info{};
	descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	if (descriptor_sets_individually_resetable) {
		descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	}

	descriptor_pool_info.poolSizeCount = pool_sizes.size();
	descriptor_pool_info.pPoolSizes = pool_sizes.data();
	descriptor_pool_info.maxSets = max_sets;

	if (vkCreateDescriptorPool(device, &descriptor_pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
		log_error("Failed to create descriptor pool with num sets ", max_sets, " and num descriptors per set ", max_descriptors_per_set);
	}

	return descriptor_pool;
}

void create_descriptor_sets(VkDevice device, VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts, std::span<VkDescriptorSet> out_descriptor_sets)
{
	if (layouts.size() != out_descriptor_sets.size()) {
		log_error("Failed to create descriptor sets, number of layouts must match number of descriptors. Num layouts = ", layouts.size(), ". Num descriptors = ", out_descriptor_sets.size());
		return;
	}

	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = pool;
	alloc_info.descriptorSetCount = static_cast<uint32_t>(out_descriptor_sets.size());
	alloc_info.pSetLayouts = layouts.data();

	vkAllocateDescriptorSets(device, &alloc_info, out_descriptor_sets.data());
}

FrameDescriptorSets create_frame_descriptor_sets(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout descriptor_set_layout, const FrameUniformBuffers& uniform_buffers, VkImageView texture, VkSampler texture_sampler)
{
	// Create descriptor sets using the layout specified for the render pipeline.

	FrameDescriptorSets frame_descriptor_sets{};
	create_descriptor_sets(device, pool, descriptor_set_layout, frame_descriptor_sets);

	// initialise the descriptors so that they point to the data needed.

	for (std::size_t i = 0; i < frame_descriptor_sets.size(); ++i) {
		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = uniform_buffers[i].buffer;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(UniformBufferContent);

		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture;
		image_info.sampler = texture_sampler;

		std::array<VkWriteDescriptorSet, 2> writers{};
		writers[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writers[0].dstSet = frame_descriptor_sets[i];
		writers[0].dstBinding = 0;
		writers[0].dstArrayElement = 0;
		writers[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writers[0].descriptorCount = 1;
		writers[0].pBufferInfo = &buffer_info;
		writers[0].pImageInfo = nullptr; // Optional
		writers[0].pTexelBufferView = nullptr; // Optional

		writers[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writers[1].dstSet = frame_descriptor_sets[i];
		writers[1].dstBinding = 1;
		writers[1].dstArrayElement = 0;
		writers[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writers[1].descriptorCount = 1;
		writers[1].pBufferInfo = nullptr;
		writers[1].pImageInfo = &image_info; // Optional
		writers[1].pTexelBufferView = nullptr; // Optional

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writers.size()), writers.data(), 0, nullptr);
	}
	
	return frame_descriptor_sets;
}


